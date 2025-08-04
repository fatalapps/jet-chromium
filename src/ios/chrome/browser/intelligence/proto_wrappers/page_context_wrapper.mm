// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"

#import <Foundation/Foundation.h>

#import <memory>
#import <optional>
#import <string>
#import <utility>

#import "base/barrier_closure.h"
#import "base/check.h"
#import "base/check_op.h"
#import "base/logging.h"
#import "base/memory/weak_ptr.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/token.h"
#import "components/optimization_guide/core/page_content_proto_serializer.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper_metrics.h"
#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"
#import "ios/public/provider/chrome/browser/bwg/bwg_api.h"
#import "ios/web/find_in_page/find_in_page_java_script_feature.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "url/origin.h"

namespace {

// The key for whether the PageContext should be detached. The value is a bool.
constexpr const char kShouldDetachPageContext[] = "shouldDetachPageContext";

// The key for the current node's innerText in the JavaScript object. The value
// is a string.
constexpr const char kCurrentNodeInnerTextDictKey[] = "currentNodeInnerText";

// The key for the children frames in the JavaScript object. The value is an
// array of objects.
constexpr const char kChildrenFramesDictKey[] = "children";

// The key for the source URL of the frame in the JavaScript object. The value
// is a string.
constexpr const char kSourceURLDictKey[] = "sourceURL";

// The key for the title of the frame in the JavaScript object. The value is a
// string.
constexpr const char kFrameTitleDictKey[] = "title";

// The key for the links of the frame in the JavaScript object. The value is an
// array of objects.
constexpr const char kFrameLinksDictKey[] = "links";

// The key for a link's HREF/URL field in the JavaScript object. The value is a
// string.
constexpr const char kLinkHREFDictKey[] = "href";

// The key for a link's innerText in the JavaScript object. The value is a
// string.
constexpr const char kLinkTextDictKey[] = "linkText";

// The JavaScript to be executed on each WebState's WebFrames, which retrieves
// the innerText of the document body, and recursively traverses through
// same-origin nested iframes to retrieve their innerTexts as well, constructing
// a tree structure. iframes are marked as processed with a nonce to avoid
// duplicate text from frames, but only for the current run. Early returns if
// the PageContext should be detached, or the frame is not the top-most
// same-origin frame.
// TODO(crbug.com/423681226): Write this in TypeScript and create a JS Feature
// for it.
constexpr const char16_t* kInnerTextTreeJavaScript = uR"DELIM(
(() => {
    // Checks whether the PageContext should be detached.
    const shouldDetachPageContext = () => {
      // PageContext detachment logic injected below.
      $1
    };

    // If the PageContext should be detached, early return.
    if (shouldDetachPageContext()) {
        return { shouldDetachPageContext: true };
    }

    // The script should only run if it has no same-origin parent. (The script
    // should only start execution on top-most nodes of a given origin).
    if (window.self !== window.top &&
        location.ancestorOrigins?.[0] === location.origin) {
        // Not the top-most same-origin frame, early exit.
        return null;
    }

    // Recursively constructs the innerText tree for the passed node and its
    // children same-origin iframes.
    const constructSameOriginInnerTextTree = (node, frameURL, frameTitle, nonceAttributeValue) => {
        // Early return if the node is null or already processed.
        if (!node || node.getAttribute('data-__gCrWeb-innerText-processed') === nonceAttributeValue) {
            return null;
        }

        // Mark node as processed.
        node.setAttribute('data-__gCrWeb-innerText-processed', nonceAttributeValue);

        // Get all nested iframes within the current node.
        const nestedIframes = node.getElementsByTagName('iframe');
        const childNodeInnerTexts = [...nestedIframes].map(iframe => {
            if (!iframe) {
                return null;
            }

            // Try to access the iframe's body, failure is possible (cross-origin iframes).
            let iframeBody;
            try {
                iframeBody = iframe.contentDocument ? iframe.contentDocument.body : null;
            } catch (error) {
                return null;
            }

            // Recursively construct the innerText tree for the iframe's body.
            return iframeBody ? constructSameOriginInnerTextTree(iframeBody, iframe.src, iframe.title,
                nonceAttributeValue) : null;
        });

        const result = {
            currentNodeInnerText: node.innerText,
            children: childNodeInnerTexts.filter(item => item !== null),
            sourceURL: frameURL,
            title: frameTitle,
        };

        // Anchor tag retrieval logic injected below.
        $2

        return result;
    };

    return constructSameOriginInnerTextTree(document.body, window.location.href, document.title, "$3");
})();
  )DELIM";

// The JavaScript to be executed in each WebFrame which gets all of a frame's
// anchor tags and adds them to an array with their corresponding URL and
// innerText. Injected into the main script.
constexpr const char16_t* kAnchorTagsJavaScript = uR"DELIM(
// Add all the frame's anchor tags to a links array with their HREF/URL and
// innerText.
const linksArray = [];
const anchorElements = node.querySelectorAll('a[href]');
anchorElements.forEach((anchor) => {
    linksArray.push({
        href: anchor.href,
        linkText: anchor.innerText
    });
});

result.links = linksArray;
  )DELIM";

}  // namespace

// TODO(crbug.com/424258248): Add a timeout for the execution of the async tasks
// in the PageContextWrapper.
@implementation PageContextWrapper {
  base::WeakPtr<web::WebState> _webState;

  // The amount of async tasks this specific instance of the PageContext wrapper
  // needs to complete before executing the `completionCallback`.
  NSInteger _asyncTasksToComplete;

  // The root node of the PageContext's AnnotatedPageContent (APC) tree. This
  // tree is constructed on the fly as values are returned from JavaScript.
  std::unique_ptr<optimization_guide::proto::AnnotatedPageContent> _rootAPCNode;

  // Whether the PageContext should be detached. Likely a protected page.
  BOOL _forceDetachPageContext;

  // The callback to execute once all async work is complete, whichs
  // relinquishes ownership of the PageContext proto to the callback's handler.
  base::OnceCallback<void(PageContextWrapperCallbackResponse)>
      _completionCallback;

  // Unique pointer to the PageContext proto.
  std::unique_ptr<optimization_guide::proto::PageContext> _pageContext;

  // The current PageContext instance's metrics logger. Only created when async
  // tasks execution is started.
  PageContextWrapperMetrics* _pageContextMetrics;
}

- (instancetype)initWithWebState:(web::WebState*)webState
              completionCallback:
                  (base::OnceCallback<void(PageContextWrapperCallbackResponse)>)
                      completionCallback {
  self = [super init];
  if (self) {
    _asyncTasksToComplete = 0;
    _webState = webState->GetWeakPtr();
    _completionCallback = std::move(completionCallback);

    // Create the PageContext proto/object.
    _pageContext = std::make_unique<optimization_guide::proto::PageContext>();
    _pageContext->set_url(_webState->GetVisibleURL().spec());
    _pageContext->set_title(base::UTF16ToUTF8(_webState->GetTitle()));
  }
  return self;
}

- (void)dealloc {
  [self stopTextHighlighting];
}

- (void)populatePageContextFieldsAsync {
  CHECK_GE(_asyncTasksToComplete, 0);
  _pageContextMetrics = [[PageContextWrapperMetrics alloc] init];

  if (_asyncTasksToComplete == 0) {
    [self asyncWorkCompletedForPageContext];
    return;
  }

  // Use a `BarrierClosure` to ensure all async tasks are completed before
  // executing the overall completion callback. The BarrierClosure will wait
  // until the `pageContextBarrier` callback is itself run
  // `_asyncTasksToComplete` times.
  __weak PageContextWrapper* weakSelf = self;
  base::RepeatingClosure pageContextBarrier =
      base::BarrierClosure(_asyncTasksToComplete, base::BindOnce(^{
                             [weakSelf asyncWorkCompletedForPageContext];
                           }));

  // Asynchronous work. *IMPORTANT NOTES*:
  // When adding async tasks below, an accompanying setter should also be
  // created to follow the disabled-by-default pattern (which
  // increments/decrements `_asyncTasksToComplete` accordingly). Also, if a
  // given task is enabled, every code path for that task should eventually
  // execute the `pageContextBarrier` callback, otherwise the `BarrierClosure`
  // will never execute its completion block.

  if (_shouldGetSnapshot) {
    [self processSnapshotWithBarrier:pageContextBarrier];
  }

  if (_shouldGetAnnotatedPageContent) {
    [self processAnnotatedPageContentWithBarrier:pageContextBarrier];
  }

  // Create full page PDF representation of the WebState, if enabled.
  if (_shouldGetFullPagePDF) {
    [_pageContextMetrics executionStartedForTask:PageContextTask::kPDF];

    _webState->CreateFullPagePdf(base::BindOnce(^(NSData* PDFData) {
      [weakSelf encodeAndSetFullPagePDF:PDFData];
      pageContextBarrier.Run();
    }));
  }
}

#pragma mark - Setters

// Sets the flag to enabled/disabled, and increments/decrements accordingly the
// total amount of async tasks gating the completion callback.
- (void)setShouldGetSnapshot:(BOOL)shouldGetSnapshot {
  if (_shouldGetSnapshot == shouldGetSnapshot) {
    return;
  }

  _asyncTasksToComplete += shouldGetSnapshot ? 1 : -1;
  _shouldGetSnapshot = shouldGetSnapshot;
}

// Sets the flag to enabled/disabled, and increments/decrements accordingly the
// total amount of async tasks gating the completion callback.
- (void)setShouldGetFullPagePDF:(BOOL)shouldGetFullPagePDF {
  if (_shouldGetFullPagePDF == shouldGetFullPagePDF) {
    return;
  }

  _asyncTasksToComplete += shouldGetFullPagePDF ? 1 : -1;
  _shouldGetFullPagePDF = shouldGetFullPagePDF;
}

// Sets the flag to enabled/disabled, and increments/decrements accordingly the
// total amount of async tasks gating the completion callback.
- (void)setShouldGetAnnotatedPageContent:(BOOL)shouldGetAnnotatedPageContent {
  if (_shouldGetAnnotatedPageContent == shouldGetAnnotatedPageContent) {
    return;
  }

  _asyncTasksToComplete += shouldGetAnnotatedPageContent ? 1 : -1;
  _shouldGetAnnotatedPageContent = shouldGetAnnotatedPageContent;
}

#pragma mark - Private

// Retrieve WebState snapshot. The barrier's callback will be executed for all
// codepaths in this method.
- (void)processSnapshotWithBarrier:(base::RepeatingClosure)barrier {
  [_pageContextMetrics executionStartedForTask:PageContextTask::kScreenshot];

  __weak PageContextWrapper* weakSelf = self;
  auto callback = ^(UIImage* image) {
    __strong __typeof(weakSelf) strongSelf = weakSelf;
    if (!strongSelf) {
      return;
    }

    if ([strongSelf shouldUpdateSnapshotWithImage:image]) {
      [strongSelf updateSnapshotWithBarrier:barrier];
      return;
    }

    [strongSelf encodeImageAndSetTabScreenshot:image];
    barrier.Run();
  };

  // If the WebState is currently visible, update the snapshot in case the
  // user was scrolling, otherwise retrieve the latest version in cache or on
  // disk.
  if (_webState->IsVisible()) {
    auto updateSnapshotCallback =
        base::BindOnce(^(std::optional<int> result_matches) {
          // TODO(crbug.com/401282824): Log the matches count to measure text
          // highlighting precision.
          [weakSelf updateSnapshotWithCallback:callback];
        });

    // If there is text to highlight, do it before capturing the screenshot.
    if (_textToHighlight != nil) {
      web::WebFrame* mainFrame =
          _webState->GetPageWorldWebFramesManager()->GetMainWebFrame();
      web::FindInPageJavaScriptFeature* findInPageFeature =
          web::FindInPageJavaScriptFeature::GetInstance();

      findInPageFeature->Search(mainFrame,
                                base::SysNSStringToUTF8(_textToHighlight),
                                std::move(updateSnapshotCallback));
    } else {
      std::move(updateSnapshotCallback).Run(std::nullopt);
    }
  } else {
    SnapshotTabHelper::FromWebState(_webState.get())
        ->RetrieveColorSnapshot(callback);
  }
}

// Get the WebState's AnnotatedPageContent filled with innerTexts. The barrier's
// callback will be executed for all codepaths in this method.
- (void)processAnnotatedPageContentWithBarrier:(base::RepeatingClosure)barrier {
  [_pageContextMetrics
      executionStartedForTask:PageContextTask::kAnnotatedPageContent];

  std::set<web::WebFrame*> webFrames =
      _webState->GetPageWorldWebFramesManager()->GetAllWebFrames();
  web::WebFrame* mainFrame =
      _webState->GetPageWorldWebFramesManager()->GetMainWebFrame();

  if (webFrames.empty() || !mainFrame) {
    [_pageContextMetrics
        executionFinishedForTask:PageContextTask::kAnnotatedPageContent
            withCompletionStatus:PageContextCompletionStatus::kFailure];
    barrier.Run();
    return;
  }

  // Create the root node of the APC tree and its first root ContentNode.
  _rootAPCNode =
      std::make_unique<optimization_guide::proto::AnnotatedPageContent>();
  _rootAPCNode->set_version(
      optimization_guide::proto::AnnotatedPageContentVersion::
          ANNOTATED_PAGE_CONTENT_VERSION_1_0);
  _rootAPCNode->mutable_root_node()
      ->mutable_content_attributes()
      ->set_attribute_type(optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  // Use a `BarrierClosure` to ensure the JavaScript is done executing in
  // all WebFrames before executing the page context barrier `barrier`,
  // which in turn signals to the PageContextWrapper that the APC is done being
  // processed. The BarrierClosure will wait until the
  // `annotatedPageContentBarrier` callback is itself run once per WebFrame (+1
  // since we execute the JS explicitly on the main frame first).
  __weak PageContextWrapper* weakSelf = self;
  base::RepeatingClosure annotatedPageContentBarrier = base::BarrierClosure(
      webFrames.size() + 1, base::BindOnce(^{
        [weakSelf webFramesAnnotatedPageContentFetchCompleted];
        barrier.Run();
      }));

  // Callback to aggregate values from the JS execution.
  auto callback = [](PageContextWrapper* weakWrapper,
                     base::RepeatingClosure barrier, BOOL isMainFrame,
                     const url::Origin& securityOrigin,
                     const base::Value* value, NSError* error) {
    [weakWrapper aggregateJavaScriptValue:value
                                withError:error
                              isMainFrame:isMainFrame
                           securityOrigin:securityOrigin];
    barrier.Run();
  };

  // Construct the JavaScript script to be executed on each Web Frame with a
  // random token as nonce to differentiate between runs/executions.
  base::Token nonce = base::Token::CreateRandom();
  std::u16string nonceString = base::UTF8ToUTF16(nonce.ToString());
  std::u16string maybeAnchorTagsJavaScript =
      IsPageContextAnchorTagsEnabled() ? kAnchorTagsJavaScript : u"";
  std::u16string script = base::ReplaceStringPlaceholders(
      kInnerTextTreeJavaScript,
      base::span<const std::u16string>(
          {ios::provider::GetPageContextShouldDetachScript(),
           maybeAnchorTagsJavaScript, nonceString}),
      nullptr);

  // Execute the JavaScript on the main WebFrame first and pass in the callback
  // (which executes the barrier when run)
  mainFrame->ExecuteJavaScript(
      script,
      base::BindOnce(callback, weakSelf, annotatedPageContentBarrier,
                     /*isMainFrame=*/YES, mainFrame->GetSecurityOrigin()));

  // Execute the JavaScript on each other WebFrame and pass in the callback
  // (which executes the barrier when run).
  for (web::WebFrame* webFrame : webFrames) {
    // Skip the main frame since it was already processed above.
    if (!webFrame || webFrame->IsMainFrame()) {
      annotatedPageContentBarrier.Run();
      continue;
    }

    webFrame->ExecuteJavaScript(
        script,
        base::BindOnce(callback, weakSelf, annotatedPageContentBarrier,
                       /*isMainFrame=*/NO, webFrame->GetSecurityOrigin()));
  }
}

// All async tasks are complete, execute the overall completion callback.
// Relinquish ownership to the callback handler.
- (void)asyncWorkCompletedForPageContext {
  [self stopTextHighlighting];

  PageContextWrapperCallbackResponse response;
  PageContextCompletionStatus completionStatus;

  // Construct the response and completion status, either with the expected
  // value or an error.
  if (_forceDetachPageContext) {
    response = base::unexpected(PageContextWrapperError::kForceDetachError);
    completionStatus = PageContextCompletionStatus::kProtected;
  } else if (_shouldGetAnnotatedPageContent &&
             !_pageContext->has_annotated_page_content()) {
    response = base::unexpected(PageContextWrapperError::kAPCError);
    completionStatus = PageContextCompletionStatus::kFailure;
  } else if (_shouldGetSnapshot && !_pageContext->has_tab_screenshot()) {
    response = base::unexpected(PageContextWrapperError::kScreenshotError);
    completionStatus = PageContextCompletionStatus::kFailure;

  } else if (_shouldGetFullPagePDF && !_pageContext->has_pdf_data()) {
    response = base::unexpected(PageContextWrapperError::kPDFDataError);
    completionStatus = PageContextCompletionStatus::kFailure;

  } else {
    response = base::ok(std::move(_pageContext));
    completionStatus = PageContextCompletionStatus::kSuccess;
  }

  [_pageContextMetrics executionFinishedForTask:PageContextTask::kOverall
                           withCompletionStatus:completionStatus];

  std::move(_completionCallback).Run(std::move(response));
}

// Returns YES if the image is nil and forcing the update of missing snapshots
// is enabled.
- (BOOL)shouldUpdateSnapshotWithImage:(UIImage*)image {
  return !image && _shouldForceUpdateMissingSnapshots;
}

// Updates the snapshot for the given WebState, and executes the `barrier`
// callback when finished.
- (void)updateSnapshotWithBarrier:(base::RepeatingClosure)barrier {
  __weak PageContextWrapper* weakSelf = self;
  SnapshotTabHelper::FromWebState(_webState.get())
      ->UpdateSnapshotWithCallback(^(UIImage* image) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if (!strongSelf) {
          return;
        }
        [strongSelf encodeImageAndSetTabScreenshot:image];
        barrier.Run();
      });
}

// Updates the current WebState's snapshot with the given callback.
- (void)updateSnapshotWithCallback:(void (^)(UIImage*))callback {
  if (_webState) {
    SnapshotTabHelper::FromWebState(_webState.get())
        ->UpdateSnapshotWithCallback(callback);
  }
}

// Convert UIImage snapshot to PNG, and then to base64 encoded string. Set the
// tab screenshot on the current PageContext.
- (void)encodeImageAndSetTabScreenshot:(UIImage*)image {
  [self stopTextHighlighting];

  if (!image) {
    [_pageContextMetrics
        executionFinishedForTask:PageContextTask::kScreenshot
            withCompletionStatus:PageContextCompletionStatus::kFailure];
    DLOG(WARNING) << "Failed to fetch webpage screenshot.";
    return;
  }

  NSData* imageData = UIImagePNGRepresentation(image);
  if (!imageData) {
    [_pageContextMetrics
        executionFinishedForTask:PageContextTask::kScreenshot
            withCompletionStatus:PageContextCompletionStatus::kFailure];
    DLOG(WARNING) << "Failed to convert the screenshot to PNG.";
    return;
  }

  NSString* base64String = [imageData base64EncodedStringWithOptions:0];
  _pageContext->set_tab_screenshot(base::SysNSStringToUTF8(base64String));

  [_pageContextMetrics
      executionFinishedForTask:PageContextTask::kScreenshot
          withCompletionStatus:PageContextCompletionStatus::kSuccess];
}

// If it exists, convert the PDF data to base64 encoded string and set it in the
// PageContext proto.
- (void)encodeAndSetFullPagePDF:(NSData*)PDFData {
  if (!PDFData) {
    [_pageContextMetrics
        executionFinishedForTask:PageContextTask::kPDF
            withCompletionStatus:PageContextCompletionStatus::kFailure];
    DLOG(WARNING) << "Failed to fetch webpage PDF data.";
    return;
  }

  NSString* base64String = [PDFData base64EncodedStringWithOptions:0];
  _pageContext->set_pdf_data(base::SysNSStringToUTF8(base64String));

  [_pageContextMetrics
      executionFinishedForTask:PageContextTask::kPDF
          withCompletionStatus:PageContextCompletionStatus::kSuccess];
}

// If it exists, parse the returned JavaScript value from the WebFrame,
// construct its ContentNode subtree and insert it into the APC tree.
- (void)aggregateJavaScriptValue:(const base::Value*)value
                       withError:(NSError*)error
                     isMainFrame:(BOOL)isMainFrame
                  securityOrigin:(const url::Origin&)securityOrigin {
  if (error || !value || !value->is_dict()) {
    DLOG(WARNING) << "Failed to fetch frame's innerText tree.";
    if (error) {
      // TODO(crbug.com/401282824): Log the failure rate of aggregation.
      DLOG(WARNING) << base::SysNSStringToUTF8([error localizedDescription]);
    }
    return;
  }

  if (_forceDetachPageContext) {
    return;
  }

  // Check if PageContext should be force detached.
  // TODO(crbug.com/423681226): Force detaching PageContext shouldn't depend on
  // fetching innerText/APC, it should always be enabled.
  std::optional<bool> shouldDetachPageContext =
      value->GetDict().FindBool(kShouldDetachPageContext);
  if (shouldDetachPageContext.has_value() && shouldDetachPageContext.value()) {
    _forceDetachPageContext = YES;
    [_pageContextMetrics
        executionFinishedForTask:PageContextTask::kAnnotatedPageContent
            withCompletionStatus:PageContextCompletionStatus::kProtected];
    return;
  }

  if (isMainFrame) {
    [self populateMainFrameSubtreeWithValue:value origin:securityOrigin];
  }

  // Recursively populate the ContentNode subtree for any of the WebFrame's
  // iframes.
  const base::Value::List* childrenFrames =
      value->GetDict().FindList(kChildrenFramesDictKey);
  if (childrenFrames && !childrenFrames->empty()) {
    for (const auto& childFrame : *childrenFrames) {
      if (!childFrame.is_dict()) {
        continue;
      }

      [self populateIframeSubtreeWithValue:&childFrame
                                    origin:securityOrigin
                                parentNode:_rootAPCNode->mutable_root_node()];
    }
  }
}

// Set the constructed APC tree on the PageContext proto.
- (void)webFramesAnnotatedPageContentFetchCompleted {
  _pageContext->set_allocated_annotated_page_content(_rootAPCNode.release());

  [_pageContextMetrics
      executionFinishedForTask:PageContextTask::kAnnotatedPageContent
          withCompletionStatus:PageContextCompletionStatus::kSuccess];
}

// Populate the main frame's ContentNode subtree with the correct nodes and
// their values. Adds Main Frame data and the root text ContentNode.
- (void)populateMainFrameSubtreeWithValue:(const base::Value*)value
                                   origin:(const url::Origin&)origin {
  if (!value || !value->is_dict()) {
    return;
  }

  // Set the main frame's security origin.
  [self populateFrameDataNode:_rootAPCNode->mutable_main_frame_data()
                    withValue:value
                       origin:origin];

  // Set its child text node.
  [self populateTextInfoNodeWithValue:value
                               origin:origin
                           parentNode:_rootAPCNode->mutable_root_node()];

  // Set its children anchor nodes.
  if (IsPageContextAnchorTagsEnabled()) {
    [self
        populateAnchorNodeChildrenWithValue:value
                                 parentNode:_rootAPCNode->mutable_root_node()];
  }
}

//  Populate a FrameData node with the correct values.
- (void)populateFrameDataNode:
            (optimization_guide::proto::FrameData*)frameDataNode
                    withValue:(const base::Value*)value
                       origin:(const url::Origin&)origin {
  if (!value || !value->is_dict() || !frameDataNode) {
    return;
  }

  optimization_guide::SecurityOriginSerializer::Serialize(
      origin, frameDataNode->mutable_security_origin());

  const std::string* titlePtr = value->GetDict().FindString(kFrameTitleDictKey);
  if (titlePtr) {
    frameDataNode->set_title(*titlePtr);
  }

  const std::string* urlPtr = value->GetDict().FindString(kSourceURLDictKey);
  if (urlPtr) {
    frameDataNode->set_url(*urlPtr);
  }
}

// Populate a ContentNode with a TextInfo node and its correct values.
- (void)populateTextInfoNodeWithValue:(const base::Value*)value
                               origin:(const url::Origin&)origin
                           parentNode:(optimization_guide::proto::ContentNode*)
                                          parentNode {
  if (!value || !value->is_dict() || !parentNode) {
    return;
  }

  // Early return if there is no text to add.
  const std::string* innerTextPtr =
      value->GetDict().FindString(kCurrentNodeInnerTextDictKey);
  if (!innerTextPtr) {
    return;
  }
  std::string_view trimmedText =
      base::TrimWhitespaceASCII(*innerTextPtr, base::TRIM_ALL);
  if (trimmedText.empty()) {
    return;
  }

  // Create and add the text node.
  optimization_guide::proto::ContentNode* childTextNode =
      parentNode->add_children_nodes();
  childTextNode->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  childTextNode->mutable_content_attributes()
      ->mutable_text_data()
      ->set_text_content(trimmedText);
}

// Populate the ContentNode subtree for an iframe with the correct values. Also
// recursively populates the subtrees for all of this iframe's children.
- (void)populateIframeSubtreeWithValue:(const base::Value*)value
                                origin:(const url::Origin&)origin
                            parentNode:(optimization_guide::proto::ContentNode*)
                                           parentNode {
  if (!value || !value->is_dict() || !parentNode) {
    return;
  }

  // Create the child iframe node.
  optimization_guide::proto::ContentNode* node =
      parentNode->add_children_nodes();
  node->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_IFRAME);

  // Set its FrameData values.
  optimization_guide::proto::FrameData* nodeFrameData =
      node->mutable_content_attributes()
          ->mutable_iframe_data()
          ->mutable_frame_data();
  [self populateFrameDataNode:nodeFrameData withValue:value origin:origin];

  // Create the nested root child ContentNode.
  optimization_guide::proto::ContentNode* childRootNode =
      node->add_children_nodes();
  childRootNode->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_ROOT);

  // Create the nested text node.
  [self populateTextInfoNodeWithValue:value
                               origin:origin
                           parentNode:childRootNode];

  // Create the children anchor nodes.
  if (IsPageContextAnchorTagsEnabled()) {
    [self populateAnchorNodeChildrenWithValue:value parentNode:childRootNode];
  }

  // Recursively populate the ContentNode subtree for any children iframes.
  const base::Value::List* childrenFrames =
      value->GetDict().FindList(kChildrenFramesDictKey);
  if (childrenFrames && !childrenFrames->empty()) {
    for (const auto& childFrame : *childrenFrames) {
      if (childFrame.is_dict()) {
        [self populateIframeSubtreeWithValue:&childFrame
                                      origin:origin
                                  parentNode:childRootNode];
      }
    }
  }
}

// Populate all anchor tags as AnchorData nodes which are direct children of
// `parentNode`.
- (void)populateAnchorNodeChildrenWithValue:(const base::Value*)value
                                 parentNode:
                                     (optimization_guide::proto::ContentNode*)
                                         parentNode {
  if (!value || !value->is_dict() || !parentNode) {
    return;
  }

  const base::Value::List* links =
      value->GetDict().FindList(kFrameLinksDictKey);
  if (!links || links->empty()) {
    return;
  }

  for (const auto& linkValue : *links) {
    [self populateAnchorNodeWithValue:&linkValue parentNode:parentNode];
  }
}

// Creates an AnchorData node (with the corresponding URL) with one child
// TextInfo node (with the corresponding innerText). Set the AnchorData node as
// direct child of `parentNode`.
- (void)populateAnchorNodeWithValue:(const base::Value*)linkData
                         parentNode:(optimization_guide::proto::ContentNode*)
                                        parentNode {
  if (!linkData || !linkData->is_dict() || !parentNode) {
    return;
  }

  const std::string* href = linkData->GetDict().FindString(kLinkHREFDictKey);
  if (!href || href->empty()) {
    return;
  }

  // Create the anchor node.
  optimization_guide::proto::ContentNode* anchorNode =
      parentNode->add_children_nodes();
  anchorNode->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_ANCHOR);

  // Set the anchor data (the HREF).
  anchorNode->mutable_content_attributes()->mutable_anchor_data()->set_url(
      *href);

  // Create a child text node for the anchor's innerText.
  const std::string* linkText =
      linkData->GetDict().FindString(kLinkTextDictKey);
  if (!linkText || linkText->empty() ||
      base::TrimWhitespaceASCII(*linkText, base::TRIM_ALL).empty()) {
    return;
  }

  // Set the child text node's text value.
  optimization_guide::proto::ContentNode* textNode =
      anchorNode->add_children_nodes();
  textNode->mutable_content_attributes()->set_attribute_type(
      optimization_guide::proto::CONTENT_ATTRIBUTE_TEXT);
  textNode->mutable_content_attributes()->mutable_text_data()->set_text_content(
      *linkText);
}

// Stop the highlighting of text.
- (void)stopTextHighlighting {
  if (!_webState) {
    return;
  }

  web::WebFrame* mainFrame =
      _webState->GetPageWorldWebFramesManager()->GetMainWebFrame();

  if (!mainFrame) {
    return;
  }

  web::FindInPageJavaScriptFeature* find_in_page_feature =
      web::FindInPageJavaScriptFeature::GetInstance();

  find_in_page_feature->Stop(mainFrame);
}

@end

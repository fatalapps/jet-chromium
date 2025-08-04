// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_ui_util.h"

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/task_environment.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/enterprise/util/managed_browser_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_attributes_init_params.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/signin/signin_browser_test_base.h"
#include "chrome/browser/signin/signin_promo.h"
#include "chrome/browser/signin/signin_ui_delegate.h"
#include "chrome/browser/signin/signin_ui_delegate_impl_dice.h"
#include "chrome/browser/signin/signin_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/account_id/account_id.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/accounts_mutator.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/signin/public/identity_manager/primary_account_mutator.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_id.h"
#include "google_apis/gaia/gaia_urls.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !BUILDFLAG(ENABLE_DICE_SUPPORT)
#error This file only contains DICE browser tests for now.
#endif

namespace signin_ui_util {

namespace {
const char kMainEmail[] = "main_email@example.com";
const GaiaId::Literal kMainGaiaID("main_gaia_id");
}  // namespace

using testing::_;

namespace {

// TODO(crbug.com/40834209): move out testing of SigninUiDelegateImplDice
// in a separate file.
class MockSigninUiDelegate : public SigninUiDelegateImplDice {
 public:
  MOCK_METHOD(void,
              ShowTurnSyncOnUI,
              (Profile * profile,
               signin_metrics::AccessPoint access_point,
               signin_metrics::PromoAction promo_action,
               const CoreAccountId& account_id,
               TurnSyncOnHelper::SigninAbortedMode signin_aborted_mode,
               bool is_sync_promo,
               bool user_already_signed_in),
              ());
};

}  // namespace

class SigninUiUtilTest : public base::test::WithFeatureOverride,
                         public SigninBrowserTestBase {
 public:
  SigninUiUtilTest()
      : base::test::WithFeatureOverride(
            switches::kBrowserSigninInSyncHeaderOnGaiaIntegration),
        delegate_auto_reset_(SetSigninUiDelegateForTesting(&mock_delegate_)) {}

  bool IsFixGaiaIntegrationEnabled() const { return IsParamFeatureEnabled(); }

 protected:
  // Returns the identity manager.
  signin::IdentityManager* GetIdentityManager() {
    return IdentityManagerFactory::GetForProfile(browser()->profile());
  }

  void EnableSync(const CoreAccountInfo& account_info,
                  bool is_default_promo_account) {
    EnableSyncFromMultiAccountPromo(browser()->profile(), account_info,
                                    access_point_, is_default_promo_account);
  }

  void SignIn(const CoreAccountInfo& account_info) {
    SignInFromSingleAccountPromo(browser()->profile(), account_info,
                                 access_point_);
  }

  void ExpectTurnSyncOn(signin_metrics::AccessPoint access_point,
                        signin_metrics::PromoAction promo_action,
                        const CoreAccountId& account_id,
                        TurnSyncOnHelper::SigninAbortedMode signin_aborted_mode,
                        bool is_sync_promo,
                        bool user_already_signed_in) {
    EXPECT_CALL(mock_delegate_,
                ShowTurnSyncOnUI(browser()->profile(), access_point,
                                 promo_action, account_id, signin_aborted_mode,
                                 is_sync_promo, user_already_signed_in));
  }

  void ExpectNoSigninStartedHistograms(
      const base::HistogramTester& histogram_tester) {
    histogram_tester.ExpectTotalCount("Signin.SigninStartedAccessPoint", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.SigninStartedAccessPoint.WithDefault", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.SigninStartedAccessPoint.NotDefault", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount", 0);
    histogram_tester.ExpectTotalCount(
        "Signin.SigninStartedAccessPoint.NewAccountExistingAccount", 0);
  }

  void ExpectOneSigninStartedHistograms(
      const base::HistogramTester& histogram_tester,
      signin_metrics::PromoAction expected_promo_action) {
    histogram_tester.ExpectUniqueSample("Signin.SigninStartedAccessPoint",
                                        access_point_, 1);
    switch (expected_promo_action) {
      case signin_metrics::PromoAction::PROMO_ACTION_NO_SIGNIN_PROMO:
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NotDefault", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.WithDefault", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountExistingAccount", 0);
        break;
      case signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT:
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NotDefault", 0);
        histogram_tester.ExpectUniqueSample(
            "Signin.SigninStartedAccessPoint.WithDefault", access_point_, 1);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountExistingAccount", 0);
        break;
      case signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT:
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.WithDefault", 0);
        histogram_tester.ExpectUniqueSample(
            "Signin.SigninStartedAccessPoint.NotDefault", access_point_, 1);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountExistingAccount", 0);
        break;
      case signin_metrics::PromoAction::
          PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT:
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.WithDefault", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NotDefault", 0);
        histogram_tester.ExpectUniqueSample(
            "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount",
            access_point_, 1);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountExistingAccount", 0);
        break;
      case signin_metrics::PromoAction::
          PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT:
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.WithDefault", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NotDefault", 0);
        histogram_tester.ExpectTotalCount(
            "Signin.SigninStartedAccessPoint.NewAccountNoExistingAccount", 0);
        histogram_tester.ExpectUniqueSample(
            "Signin.SigninStartedAccessPoint.NewAccountExistingAccount",
            access_point_, 1);
        break;
    }
  }

  void TestEnableSyncPromoWithExistingWebOnlyAccount() {
    CoreAccountId account_id =
        GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
            GaiaId(kMainGaiaID), kMainEmail, "refresh_token", false,
            signin_metrics::AccessPoint::kUnknown,
            signin_metrics::SourceForRefreshTokenOperation::kUnknown);

    // Verify that the primary account is not set before.
    ASSERT_FALSE(
        GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

    ExpectTurnSyncOn(
        access_point_, signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT,
        account_id, TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
        /*is_sync_promo=*/true, /*user_already_signed_in=*/false);
    EnableSync(
        GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id),
        /*is_default_promo_account=*/true);

    // Verify that the primary account has been set.
    EXPECT_TRUE(
        GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
  }

  bool WithUpdatedGaiaIntegrationEnabled() { return GetParam(); }

  signin_metrics::AccessPoint access_point_ =
      signin_metrics::AccessPoint::kBookmarkBubble;

  testing::StrictMock<MockSigninUiDelegate> mock_delegate_;
  base::AutoReset<SigninUiDelegate*> delegate_auto_reset_;
};

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(SigninUiUtilTest);

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, EnableSyncWithExistingAccount) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kUnknown,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  GetIdentityManager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id, signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kUnknown);

  for (bool is_default_promo_account : {true, false}) {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;

    ExpectNoSigninStartedHistograms(histogram_tester);
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));

    signin_metrics::PromoAction expected_promo_action =
        is_default_promo_account
            ? signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT
            : signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT;
    ExpectTurnSyncOn(signin_metrics::AccessPoint::kBookmarkBubble,
                     expected_promo_action, account_id,
                     TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT,
                     /*is_sync_promo=*/false, /*user_already_signed_in=*/true);
    EnableSync(
        GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id),
        is_default_promo_account);

    ExpectOneSigninStartedHistograms(histogram_tester, expected_promo_action);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));
  }
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, EnableSyncWithAccountThatNeedsReauth) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://example.com"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kUnknown,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  // Add an account and then put its refresh token into an error state to
  // require a reauth before enabling sync.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      GetIdentityManager(), account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  for (bool is_default_promo_account : {true, false}) {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;

    ExpectNoSigninStartedHistograms(histogram_tester);
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));

    EnableSync(
        GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id),
        is_default_promo_account);

    ExpectOneSigninStartedHistograms(
        histogram_tester,
        is_default_promo_account
            ? signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT
            : signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));

    // Verify that the active tab has the correct DICE sign-in URL.
    TabStripModel* tab_strip = browser()->tab_strip_model();
    content::WebContents* active_contents = tab_strip->GetActiveWebContents();
    ASSERT_TRUE(active_contents);
    EXPECT_EQ(signin::GetChromeSyncURLForDice(
                  {kMainEmail, GURL(google_util::kGoogleHomepageURL)}),
              active_contents->GetVisibleURL());
    tab_strip->CloseWebContentsAt(
        tab_strip->GetIndexOfWebContents(active_contents),
        TabCloseTypes::CLOSE_USER_GESTURE);
  }
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, EnableSyncForNewAccountWithNoTab) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  ExpectNoSigninStartedHistograms(histogram_tester);
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  EnableSync(CoreAccountInfo(), false /* is_default_promo_account (not used)*/);

  ExpectOneSigninStartedHistograms(
      histogram_tester, signin_metrics::PromoAction::
                            PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  // Verify that the active tab has the correct DICE sign-in URL.
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetChromeSyncURLForDice(
                {.continue_url = GURL(google_util::kGoogleHomepageURL)}),
            active_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest,
                       EnableSyncForNewAccountWithNoTabWithExisting) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
      kMainGaiaID, kMainEmail, "refresh_token", false,
      signin_metrics::AccessPoint::kUnknown,
      signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  ExpectNoSigninStartedHistograms(histogram_tester);
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  EnableSync(CoreAccountInfo(), false /* is_default_promo_account (not used)*/);

  ExpectOneSigninStartedHistograms(
      histogram_tester,
      signin_metrics::PromoAction::PROMO_ACTION_NEW_ACCOUNT_EXISTING_ACCOUNT);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, EnableSyncForNewAccountWithOneTab) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://foo/1"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  ExpectNoSigninStartedHistograms(histogram_tester);
  EXPECT_EQ(
      0, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  EnableSync(CoreAccountInfo(), false /* is_default_promo_account (not used)*/);

  ExpectOneSigninStartedHistograms(
      histogram_tester, signin_metrics::PromoAction::
                            PROMO_ACTION_NEW_ACCOUNT_NO_EXISTING_ACCOUNT);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  // Verify that the active tab has the correct DICE sign-in URL.
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetChromeSyncURLForDice(
                {.continue_url = GURL(google_util::kGoogleHomepageURL)}),
            active_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, SignInWithAlreadySignedInAccount) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://example.com"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kUnknown,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  GetIdentityManager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id, signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kUnknown);

  SignIn(GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id));

  // Verify that the primary account is still set.
  EXPECT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  // Verify that the active tab does not open the DICE sign-in URL.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  content::WebContents* active_contents = tab_strip->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(GURL("https://example.com"), active_contents->GetVisibleURL());
  tab_strip->CloseWebContentsAt(
      tab_strip->GetIndexOfWebContents(active_contents),
      TabCloseTypes::CLOSE_USER_GESTURE);
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, SignInWithAccountThatNeedsReauth) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://example.com"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kUnknown,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  // Add an account and then put its refresh token into an error state to
  // require a reauth before signing in.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      GetIdentityManager(), account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  SignIn(GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id));

  // Verify that the active tab has the correct DICE sign-in URL.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  content::WebContents* active_contents = tab_strip->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(signin::GetAddAccountURLForDice(
                kMainEmail, GURL(google_util::kGoogleHomepageURL)),
            active_contents->GetVisibleURL());
  tab_strip->CloseWebContentsAt(
      tab_strip->GetIndexOfWebContents(active_contents),
      TabCloseTypes::CLOSE_USER_GESTURE);
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, SignInForNewAccountWithNoTab) {
  SignIn(CoreAccountInfo());

  // Verify that the active tab has the correct DICE sign-in URL.
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(WithUpdatedGaiaIntegrationEnabled()
                ? signin::GetChromeSyncURLForDice(
                      {.email = std::string(),
                       .continue_url = GURL(google_util::kGoogleHomepageURL)})
                : signin::GetAddAccountURLForDice(
                      std::string(), GURL(google_util::kGoogleHomepageURL)),
            active_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, SignInForNewAccountWithOneTab) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://foo/1"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  SignIn(CoreAccountInfo());

  // Verify that the active tab has the correct DICE sign-in URL.
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_EQ(WithUpdatedGaiaIntegrationEnabled()
                ? signin::GetChromeSyncURLForDice(
                      {.email = std::string(),
                       .continue_url = GURL(google_util::kGoogleHomepageURL)})
                : signin::GetAddAccountURLForDice(
                      std::string(), GURL(google_util::kGoogleHomepageURL)),
            active_contents->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, GetOrderedAccountsForDisplay) {
  auto enable_disclaimer_on_primary_account_change_resetter =
      enterprise_util::DisableAutomaticManagementDisclaimerUntilReset(
          browser()->profile());
  signin::IdentityManager* identity_manager_empty =
      IdentityManagerFactory::GetForProfile(browser()->profile());
  std::vector<AccountInfo> accounts_empty = GetOrderedAccountsForDisplay(
      identity_manager_empty, /*restrict_to_accounts_eligible_for_sync=*/true);
  EXPECT_TRUE(accounts_empty.empty());

  // Fill with accounts.
  const char kTestEmail1[] = "me1@gmail.com";
  const char kTestEmail2[] = "me2@gmail.com";
  const char kTestEmail3[] = "me3@gmail.com";
  const char kTestEmail4[] = "me4@gmail.com";

  signin::IdentityTestEnvironment* test_env = identity_test_env();
  signin::IdentityManager* identity_manager = test_env->identity_manager();

  // The cookies are added separately in order to show behaviour in the case
  // that refresh tokens and cookies are not added at the same time.
  test_env->MakeAccountAvailable(kTestEmail1);
  test_env->MakeAccountAvailable(kTestEmail2);
  test_env->MakeAccountAvailable(kTestEmail3);
  test_env->MakeAccountAvailable(kTestEmail4);

  test_env->SetCookieAccounts(
      {{kTestEmail4, signin::GetTestGaiaIdForEmail(kTestEmail4)},
       {kTestEmail3, signin::GetTestGaiaIdForEmail(kTestEmail3)},
       {kTestEmail2, signin::GetTestGaiaIdForEmail(kTestEmail2)},
       {kTestEmail1, signin::GetTestGaiaIdForEmail(kTestEmail1)}});

  // No primary account set.
  std::vector<AccountInfo> accounts =
      GetOrderedAccountsForDisplay(identity_manager, false);

  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail4), accounts[0].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail3), accounts[1].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail2), accounts[2].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail1), accounts[3].gaia);

  // Set a primary account.
  test_env->SetPrimaryAccount(kTestEmail3, signin::ConsentLevel::kSignin);
  accounts = GetOrderedAccountsForDisplay(identity_manager, false);

  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail3), accounts[0].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail4), accounts[1].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail2), accounts[2].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail1), accounts[3].gaia);

  // Set a different primary account.
  test_env->SetPrimaryAccount(kTestEmail1, signin::ConsentLevel::kSignin);
  accounts = GetOrderedAccountsForDisplay(identity_manager, false);

  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail1), accounts[0].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail4), accounts[1].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail3), accounts[2].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail2), accounts[3].gaia);

  // Primary account should still be included if not in cookies, other accounts
  // should not.
  test_env->SetCookieAccounts(
      {{kTestEmail4, signin::GetTestGaiaIdForEmail(kTestEmail4)},
       {kTestEmail2, signin::GetTestGaiaIdForEmail(kTestEmail2)}});
  accounts = GetOrderedAccountsForDisplay(identity_manager, false);

  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail1), accounts[0].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail4), accounts[1].gaia);
  EXPECT_EQ(signin::GetTestGaiaIdForEmail(kTestEmail2), accounts[2].gaia);
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, MergeDiceSigninTab) {
  base::UserActionTester user_action_tester;
  EnableSync(CoreAccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  // Signin tab is reused.
  EnableSync(CoreAccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));

  // Give focus to a different tab.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ASSERT_EQ(0, tab_strip->active_index());
  GURL other_url = GURL("https://example.com");

  ui_test_utils::NavigateToURLWithDisposition(
      browser(), other_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  tab_strip->ActivateTabAt(
      1, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  ASSERT_EQ(other_url, tab_strip->GetActiveWebContents()->GetVisibleURL());
  ASSERT_EQ(1, tab_strip->active_index());

  // Extensions reuse the tab but do not take focus.
  access_point_ = signin_metrics::AccessPoint::kExtensions;
  EnableSync(CoreAccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));
  EXPECT_EQ(2, tab_strip->active_index());

  // Other access points reuse the tab and take focus.
  access_point_ = signin_metrics::AccessPoint::kSettings;
  EnableSync(CoreAccountInfo(), false);
  EXPECT_EQ(
      1, user_action_tester.GetActionCount("Signin_Signin_FromBookmarkBubble"));
  EXPECT_EQ(2, tab_strip->active_index());
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, ShowReauthTab) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("http://example.com"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  AccountInfo account_info = signin::MakePrimaryAccountAvailable(
      GetIdentityManager(), "foo@example.com", signin::ConsentLevel::kSync);

  // Add an account and then put its refresh token into an error state to
  // require a reauth before enabling sync.
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      GetIdentityManager(), account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  signin_ui_util::ShowReauthForPrimaryAccountWithAuthError(
      browser()->profile(), signin_metrics::AccessPoint::kAvatarBubbleSignIn);

  // Verify that the active tab has the correct DICE sign-in URL.
  TabStripModel* tab_strip = browser()->tab_strip_model();
  content::WebContents* active_contents = tab_strip->GetActiveWebContents();
  ASSERT_TRUE(active_contents);
  EXPECT_THAT(
      active_contents->GetVisibleURL().spec(),
      testing::StartsWith(GaiaUrls::GetInstance()->add_account_url().spec()));
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, ShowExtensionSigninPrompt) {
  const GURL sync_url = GaiaUrls::GetInstance()->signin_chrome_sync_dice();

  Profile* profile = browser()->profile();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/true,
                            /*email_hint=*/std::string());
  EXPECT_EQ(1, tab_strip->count());
  // Calling the function again reuses the tab.
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/true,
                            /*email_hint=*/std::string());
  EXPECT_EQ(1, tab_strip->count());

  content::WebContents* tab = tab_strip->GetWebContentsAt(0);
  ASSERT_TRUE(tab);
  EXPECT_TRUE(base::StartsWith(tab->GetVisibleURL().spec(), sync_url.spec(),
                               base::CompareCase::INSENSITIVE_ASCII));

  // Changing the parameter opens a new tab.
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/false,
                            /*email_hint=*/std::string());
  EXPECT_EQ(2, tab_strip->count());
  // Calling the function again reuses the tab.
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/false,
                            /*email_hint=*/std::string());
  EXPECT_EQ(2, tab_strip->count());
  tab = tab_strip->GetWebContentsAt(1);
  ASSERT_TRUE(tab);
  // With explicit signin, `sync_url` is used even though Sync is not going to
  // be enabled. This is because that web page displays additional text
  // explaining to the user that they are signing in to Chrome.
  EXPECT_TRUE(base::StartsWith(tab->GetVisibleURL().spec(), sync_url.spec(),
                               base::CompareCase::INSENSITIVE_ASCII));
  EXPECT_NE(tab->GetVisibleURL().query().find("flow=promo"), std::string::npos);
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest,
                       ShowExtensionSigninPrompt_AsLockedProfile) {
  signin_util::ScopedForceSigninSetterForTesting force_signin_setter(true);
  Profile* profile = browser()->profile();
  ProfileAttributesEntry* entry =
      g_browser_process->profile_manager()
          ->GetProfileAttributesStorage()
          .GetProfileAttributesWithPath(profile->GetPath());
  ASSERT_NE(entry, nullptr);
  entry->LockForceSigninProfile(true);
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/true,
                            /*email_hint=*/std::string());
  EXPECT_EQ(1, tab_strip->count());
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/false,
                            /*email_hint=*/std::string());
  EXPECT_EQ(1, tab_strip->count());
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, ShowSigninPromptFromPromo) {
  Profile* profile = browser()->profile();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ShowSigninPromptFromPromo(profile, access_point_);
  EXPECT_EQ(1, tab_strip->count());

  content::WebContents* tab = tab_strip->GetWebContentsAt(0);
  ASSERT_TRUE(tab);
  EXPECT_TRUE(base::StartsWith(
      tab->GetVisibleURL().spec(),
      WithUpdatedGaiaIntegrationEnabled()
          ? GaiaUrls::GetInstance()->signin_chrome_sync_dice().spec()
          : GaiaUrls::GetInstance()->add_account_url().spec(),
      base::CompareCase::INSENSITIVE_ASCII));
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest,
                       ShowSigninPromptFromPromoWithExistingAccount) {
  signin::MakePrimaryAccountAvailable(GetIdentityManager(), "foo@example.com",
                                      signin::ConsentLevel::kSignin);

  Profile* profile = browser()->profile();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());
  ShowSigninPromptFromPromo(profile, access_point_);
  EXPECT_EQ(1, tab_strip->count());
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, GetSignInTabWithAccessPoint) {
  signin::MakePrimaryAccountAvailable(GetIdentityManager(), "foo@example.com",
                                      signin::ConsentLevel::kSignin);

  Profile* profile = browser()->profile();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip->count());

  // Add tabs.
  ShowReauthForAccount(profile, "test1@gmail.com",
                       signin_metrics::AccessPoint::kSettings);
  ShowReauthForAccount(
      profile, "test2@gmail.com",
      signin_metrics::AccessPoint::kChromeSigninInterceptBubble);
  ShowReauthForAccount(profile, "test3@gmail.com",
                       signin_metrics::AccessPoint::kPasswordBubble);
  EXPECT_EQ(3, tab_strip->count());

  // Look for existing tab.
  content::WebContents* sign_in_tab = GetSignInTabWithAccessPoint(
      browser(), signin_metrics::AccessPoint::kChromeSigninInterceptBubble);
  EXPECT_EQ(signin::GetAddAccountURLForDice(
                "test2@gmail.com", GURL(google_util::kGoogleHomepageURL)),
            sign_in_tab->GetVisibleURL());

  // Look for non existing tab.
  sign_in_tab = GetSignInTabWithAccessPoint(
      browser(), signin_metrics::AccessPoint::kForcedSignin);
  EXPECT_EQ(nullptr, sign_in_tab);

  // Two tabs with the same access point, will return the first tab found.
  ShowReauthForAccount(profile, "test4@gmail.com",
                       signin_metrics::AccessPoint::kSettings);
  EXPECT_EQ(4, tab_strip->count());

  sign_in_tab = GetSignInTabWithAccessPoint(
      browser(), signin_metrics::AccessPoint::kSettings);
  EXPECT_EQ(signin::GetAddAccountURLForDice(
                "test1@gmail.com", GURL(google_util::kGoogleHomepageURL)),
            sign_in_tab->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, EnableSyncWithExistingWebOnlyAccount) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kUnknown,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  for (bool is_default_promo_account : {true, false}) {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;

    ExpectNoSigninStartedHistograms(histogram_tester);
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));

    signin_metrics::PromoAction expected_promo_action =
        is_default_promo_account
            ? signin_metrics::PromoAction::PROMO_ACTION_WITH_DEFAULT
            : signin_metrics::PromoAction::PROMO_ACTION_NOT_DEFAULT;
    ExpectTurnSyncOn(
        signin_metrics::AccessPoint::kBookmarkBubble, expected_promo_action,
        account_id,
        TurnSyncOnHelper::SigninAbortedMode::KEEP_ACCOUNT_ON_WEB_ONLY,
        /*is_sync_promo=*/false, /*user_already_signed_in=*/false);
    EnableSync(
        GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id),
        is_default_promo_account);

    ExpectOneSigninStartedHistograms(histogram_tester, expected_promo_action);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Signin_Signin_FromBookmarkBubble"));
  }
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest,
                       EnableSyncPromoWithExistingWebOnlyAccountAvatarBubble) {
  access_point_ = signin_metrics::AccessPoint::kAvatarBubbleSignInWithSyncPromo;

  TestEnableSyncPromoWithExistingWebOnlyAccount();
}

// Checks that sync is treated as a promo for kSettings.
IN_PROC_BROWSER_TEST_P(SigninUiUtilTest,
                       EnableSyncPromoWithExistingWebOnlyAccountSettings) {
  access_point_ = signin_metrics::AccessPoint::kSettings;

  TestEnableSyncPromoWithExistingWebOnlyAccount();
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, SignInWithExistingWebOnlyAccount) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kUnknown,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);

  // Verify that the primary account is not set before.
  EXPECT_FALSE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));

  SignIn(GetIdentityManager()->FindExtendedAccountInfoByAccountId(account_id));

  // Verify that the primary account has been set.
  EXPECT_TRUE(
      GetIdentityManager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}

IN_PROC_BROWSER_TEST_P(SigninUiUtilTest, ShowExtensionSigninPromptReauth) {
  CoreAccountId account_id =
      GetIdentityManager()->GetAccountsMutator()->AddOrUpdateAccount(
          kMainGaiaID, kMainEmail, "refresh_token", false,
          signin_metrics::AccessPoint::kUnknown,
          signin_metrics::SourceForRefreshTokenOperation::kUnknown);
  GetIdentityManager()->GetPrimaryAccountMutator()->SetPrimaryAccount(
      account_id, signin::ConsentLevel::kSignin,
      signin_metrics::AccessPoint::kUnknown);
  signin::UpdatePersistentErrorOfRefreshTokenForAccount(
      GetIdentityManager(), account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));

  Profile* profile = browser()->profile();
  TabStripModel* tab_strip = browser()->tab_strip_model();
  ShowExtensionSigninPrompt(profile, /*enable_sync=*/false, kMainEmail);
  EXPECT_EQ(1, tab_strip->count());

  content::WebContents* tab = tab_strip->GetWebContentsAt(0);
  ASSERT_TRUE(tab);
  EXPECT_TRUE(
      base::StartsWith(tab->GetVisibleURL().spec(),
                       GaiaUrls::GetInstance()->add_account_url().spec(),
                       base::CompareCase::INSENSITIVE_ASCII));
}

IN_PROC_BROWSER_TEST_P(
    SigninUiUtilTest,
    ShouldShowAnimatedIdentityOnOpeningWindowIfMultipleWindowsAtStartup) {
  EXPECT_TRUE(ShouldShowAnimatedIdentityOnOpeningWindow(*browser()->profile()));
  // Record that the identity was shown.
  RecordAnimatedIdentityTriggered(browser()->profile());
  // The identity can be shown again immediately (which is what happens if there
  // is multiple windows at startup).
  EXPECT_TRUE(ShouldShowAnimatedIdentityOnOpeningWindow(*browser()->profile()));
}

class DiceSigninUiUtilBrowserTest : public InProcessBrowserTest {
 public:
  DiceSigninUiUtilBrowserTest() = default;
  ~DiceSigninUiUtilBrowserTest() override = default;

  Profile* CreateProfile() {
    Profile* new_profile = nullptr;
    base::RunLoop run_loop;
    ProfileManager::CreateMultiProfileAsync(
        u"test_profile", /*icon_index=*/0, /*is_hidden=*/false,
        base::BindLambdaForTesting([&new_profile, &run_loop](Profile* profile) {
          ASSERT_TRUE(profile);
          new_profile = profile;
          run_loop.Quit();
        }));
    run_loop.Run();
    return new_profile;
  }

 private:
};

// Tests that `ShowExtensionSigninPrompt()` doesn't crash when it cannot create
// a new browser. Regression test for https://crbug.com/1273370.
IN_PROC_BROWSER_TEST_F(DiceSigninUiUtilBrowserTest,
                       ShowExtensionSigninPrompt_NoBrowser) {
  Profile* new_profile = CreateProfile();

  // New profile should not have any browser windows.
  EXPECT_FALSE(chrome::FindBrowserWithProfile(new_profile));

  ShowExtensionSigninPrompt(new_profile, /*enable_sync=*/false,
                            /*email_hint=*/std::string());
  // `ShowExtensionSigninPrompt()` creates a new browser.
  Browser* browser = chrome::FindBrowserWithProfile(new_profile);
  ASSERT_TRUE(browser);
  EXPECT_EQ(1, browser->tab_strip_model()->count());

  // Profile deletion closes the browser.
  g_browser_process->profile_manager()
      ->GetDeleteProfileHelper()
      .MaybeScheduleProfileForDeletion(
          new_profile->GetPath(), base::DoNothing(),
          ProfileMetrics::DELETE_PROFILE_USER_MANAGER);
  ui_test_utils::WaitForBrowserToClose(browser);
  EXPECT_FALSE(chrome::FindBrowserWithProfile(new_profile));

  // `ShowExtensionSigninPrompt()` does nothing for deleted profile.
  ShowExtensionSigninPrompt(new_profile, /*enable_sync=*/false,
                            /*email_hint=*/std::string());
  EXPECT_FALSE(chrome::FindBrowserWithProfile(new_profile));
}

}  // namespace signin_ui_util

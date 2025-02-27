// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/content_suggestions/ntp_home_constant.h"
#import "ios/chrome/browser/ui/ntp/new_tab_page_controller.h"
#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_row.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/clean/toolbar_view.h"
#import "ios/chrome/browser/ui/toolbar/legacy/toolbar_controller.h"
#include "ios/chrome/browser/ui/tools_menu/public/tools_menu_constants.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/test/http_server/http_server_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::OmniboxText;
using chrome_test_util::SystemSelectionCallout;
using chrome_test_util::SystemSelectionCalloutCopyButton;

namespace {
// Returns matcher for the Paste and Go button on the system callout.
id<GREYMatcher> PasteAndGoCalloutButton() {
  return grey_allOf(
      grey_accessibilityLabel(@"Paste and Go"),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitButton)),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitStaticText)), nil);
}

}  // namespace

// Toolbar integration tests for Chrome.
@interface ToolbarTestCase : ChromeTestCase
@end

@implementation ToolbarTestCase

#pragma mark Tests

// Verifies that entering a URL in the omnibox navigates to the correct URL and
// displays content.
- (void)testEnterURL {
  web::test::SetUpFileBasedHttpServer();
  const GURL URL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/destination.html");
  [ChromeEarlGrey loadURL:URL];
  [[EarlGrey selectElementWithMatcher:OmniboxText(URL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForWebViewContainingText:"You've arrived"];
}

// Verifies opening a new tab from the tools menu.
- (void)testNewTabFromMenu {
  [ChromeEarlGrey waitForMainTabCount:1];

  // Open tab via the UI.
  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> newTabButtonMatcher =
      grey_accessibilityID(kToolsMenuNewTabId);
  [[EarlGrey selectElementWithMatcher:newTabButtonMatcher]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForMainTabCount:2];
}

// Verifies opening a new incognito tab from the tools menu.
- (void)testNewIncognitoTabFromMenu {
  [ChromeEarlGrey waitForIncognitoTabCount:0];

  // Open incognito tab.
  [ChromeEarlGreyUI openToolsMenu];
  id<GREYMatcher> newIncognitoTabButtonMatcher =
      grey_accessibilityID(kToolsMenuNewIncognitoTabId);
  [[EarlGrey selectElementWithMatcher:newIncognitoTabButtonMatcher]
      performAction:grey_tap()];

  [ChromeEarlGrey waitForIncognitoTabCount:1];
}

// Tests whether input mode in an omnibox can be canceled via "Cancel" button
// and asserts it doesn't commit the omnibox contents if the input is canceled.
- (void)testToolbarOmniboxCancel {
  // Handset only (tablet does not have cancel button).
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Test not support on iPad");
  }

  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");

  [ChromeEarlGrey loadURL:URL];

  if (IsRefreshLocationBarEnabled()) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
        performAction:grey_tap()];
  }
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(URL.GetContent())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"foo")];

  id<GREYMatcher> cancelButton =
      IsUIRefreshPhase1Enabled()
          ? grey_accessibilityID(kToolbarCancelOmniboxEditButtonIdentifier)
          : grey_allOf(chrome_test_util::CancelButton(),
                       grey_not(grey_accessibilityID(@"Typing Shield")), nil);
  [[EarlGrey selectElementWithMatcher:cancelButton] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(URL.GetContent())];
}

// Tests whether input mode in an omnibox can be canceled via "hide keyboard"
// button and asserts it doesn't commit the omnibox contents if the input is
// canceled.
- (void)testToolbarOmniboxHideKeyboard {
  // TODO(crbug.com/642559): Enable the test for iPad when typing bug is fixed.
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to a simulator bug.");
  }

  // Tablet only (handset keyboard does not have "hide keyboard" button).
  if (!IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Test not support on iPhone");
  }

  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");

  [ChromeEarlGrey loadURL:URL];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(URL.GetContent())];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"foo")];

  id<GREYMatcher> hideKeyboard = grey_accessibilityLabel(@"Hide keyboard");
  [[EarlGrey selectElementWithMatcher:hideKeyboard] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(URL.GetContent())];
}

// Tests whether input mode in an omnibox can be canceled via tapping the typing
// shield and asserts it doesn't commit the omnibox contents if the input is
// canceled.
- (void)testToolbarOmniboxTypingShield {
  // Tablet only (handset keyboard does not have "hide keyboard" button).
  if (!IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Test not support on iPhone");
  }

  // TODO(crbug.com/753098): Re-enable this test on iOS 11 iPad once
  // grey_typeText works on iOS 11.
  if (base::ios::IsRunningOnIOS11OrLater()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 11.");
  }

  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");

  [ChromeEarlGrey loadURL:URL];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(URL.GetContent())];
  [ChromeEarlGreyUI focusOmniboxAndType:@"foo"];

  id<GREYMatcher> typingShield = grey_accessibilityID(@"Typing Shield");
  [[EarlGrey selectElementWithMatcher:typingShield] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(URL.GetContent())];
}

// Verifies the existence and state of toolbar UI elements.
- (void)testPreRefreshToolbarUI {
  if (IsUIRefreshPhase1Enabled()) {
    EARL_GREY_TEST_SKIPPED(
        @"This test is specific to the pre-refresh toolbar layout");
  }

  id<GREYMatcher> reloadButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(IDS_IOS_ACCNAME_RELOAD);
  id<GREYMatcher> bookmarkButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(IDS_TOOLTIP_STAR);
  id<GREYMatcher> voiceSearchButton =
      grey_allOf(chrome_test_util::ButtonWithAccessibilityLabelId(
                     IDS_IOS_ACCNAME_VOICE_SEARCH),
                 grey_ancestor(grey_kindOfClass([ToolbarView class])), nil);
  NSString* ntpOmniboxLabel = l10n_util::GetNSString(IDS_OMNIBOX_EMPTY_HINT);
  NSString* focusedOmniboxLabel = l10n_util::GetNSString(IDS_ACCNAME_LOCATION);
  NSString* omniboxLabel =
      IsIPadIdiom() ? focusedOmniboxLabel : ntpOmniboxLabel;
  id<GREYMatcher> locationbarButton =
      grey_allOf(grey_accessibilityLabel(omniboxLabel),
                 grey_minimumVisiblePercent(0.2), nil);

  [[EarlGrey selectElementWithMatcher:locationbarButton]
      assertWithMatcher:grey_sufficientlyVisible()];

  if (IsIPadIdiom()) {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ForwardButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:reloadButton]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:bookmarkButton]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:voiceSearchButton]
        assertWithMatcher:grey_sufficientlyVisible()];
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ShowTabsButton()]
        assertWithMatcher:grey_sufficientlyVisible()];
  }

  // Navigate to a page and verify the back button is enabled.
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      assertWithMatcher:grey_interactable()];
}

// Verifies that the keyboard is properly dismissed when a toolbar button
// is pressed (iPad specific).
- (void)testIPadKeyboardDismissOnButtonPress {
  // Tablet only (handset keyboard does not have "hide keyboard" button).
  if (!IsIPadIdiom()) {
    EARL_GREY_TEST_SKIPPED(@"Test not supported on iPhone");
  }

  // Load a webpage so that the "Back" button is tappable. Then load a second
  // page so that the test can go back once without ending up on the NTP.
  // (Subsequent steps in this test require the omnibox to be tappable, but in
  // some configurations the NTP only has a fakebox and does not display the
  // omnibox.)
  [ChromeEarlGrey loadURL:GURL("about:blank")];
  [ChromeEarlGrey loadURL:GURL("chrome://version")];

  // First test: check that the keyboard is opened when tapping the omnibox,
  // and that it is dismissed when the "Back" button is tapped.
  if (IsRefreshLocationBarEnabled()) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
        performAction:grey_tap()];
  }

  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Typing Shield")]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Typing Shield")]
      assertWithMatcher:grey_notVisible()];

  // Second test: check that the keyboard is opened when tapping the omnibox,
  // and that it is dismissed when the tools menu button is tapped.
  if (IsRefreshLocationBarEnabled()) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
        performAction:grey_tap()];
  }
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Typing Shield")]
      assertWithMatcher:grey_notNil()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:grey_accessibilityID(@"Typing Shield")]
      assertWithMatcher:grey_notVisible()];
}

// Verifies that copying and pasting a URL includes the hidden protocol prefix.
- (void)testCopyPasteURL {
  if (IsRefreshLocationBarEnabled()) {
    // TODO(crbug.com/834345): Enable this test when long press on the steady
    // location bar is supported.
    EARL_GREY_TEST_SKIPPED(@"Test not supported yet in UI Refresh.");
  }

  // Clear generalPasteboard before and after the test.
  [UIPasteboard generalPasteboard].string = @"";
  [self setTearDownHandler:^{
    [UIPasteboard generalPasteboard].string = @"";
  }];

  std::map<GURL, std::string> responses;
  // The URL needs to be long enough so the tap to the omnibox selects it.
  const GURL URL = web::test::HttpServer::MakeUrl("http://veryLongURLTestPage");
  const GURL secondURL = web::test::HttpServer::MakeUrl("http://pastePage");

  [ChromeEarlGrey loadURL:URL];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(URL.GetContent())];

  if (base::ios::IsRunningOnIOS11OrLater()) {
    // Can't access share menu from xctest on iOS 11+, so use the text field
    // callout bar instead.
    if (IsRefreshLocationBarEnabled()) {
      [[EarlGrey
          selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
          performAction:grey_tap()];
    } else {
      [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
          performAction:grey_tap()];
    }
    // Tap twice to get the pre-edit label callout bar copy button.
    [[EarlGrey
        selectElementWithMatcher:grey_allOf(
                                     grey_ancestor(chrome_test_util::Omnibox()),
                                     grey_kindOfClass([UILabel class]), nil)]
        performAction:grey_tap()];

    [[[EarlGrey selectElementWithMatcher:SystemSelectionCalloutCopyButton()]
        inRoot:SystemSelectionCallout()] performAction:grey_tap()];

    if (IsIPadIdiom()) {
      [[EarlGrey
          selectElementWithMatcher:grey_accessibilityID(@"Typing Shield")]
          performAction:grey_tap()];

    } else {
      // Typing shield might be unavailable if there are any suggestions
      // displayed in the popup.
      [[EarlGrey
          selectElementWithMatcher:
              grey_accessibilityID(kToolbarCancelOmniboxEditButtonIdentifier)]
          performAction:grey_tap()];
    }

  } else {
    [ChromeEarlGreyUI openShareMenu];
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabel(
                                     @"Copy")] performAction:grey_tap()];
  }

  [ChromeEarlGrey loadURL:secondURL];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_longPress()];

  id<GREYMatcher> pasteBarButton = grey_allOf(
      grey_accessibilityLabel(@"Paste"),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitButton)),
      grey_not(grey_accessibilityTrait(UIAccessibilityTraitStaticText)), nil);
  [[EarlGrey selectElementWithMatcher:pasteBarButton] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText(URL.spec().c_str())];
}

// Verifies that PasteAndGo removes javascript scheme from the URL before
// navigation.
- (void)testJavascriptPasteAndGo {
  if (!IsUIRefreshPhase1Enabled()) {
    EARL_GREY_TEST_SKIPPED(@"Paste and Go is only availble for UI Refresh.");
  }
  // Clear generalPasteboard after the test.
  [self setTearDownHandler:^{
    [UIPasteboard generalPasteboard].string = @"";
  }];
  [UIPasteboard generalPasteboard].string = @"javascript:notavalidurl";
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NewTabPageOmnibox()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_longPress()];
  [[EarlGrey selectElementWithMatcher:PasteAndGoCalloutButton()]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxContainingText(
                            "www.google.com/search?q=notavalidurl")];
}

// Verifies that the clear text button clears any text in the omnibox.
- (void)testOmniboxClearTextButton {
  // TODO(crbug.com/753098): Re-enable this test on iOS 11 iPad once
  // grey_typeText works on iOS 11.
  if (base::ios::IsRunningOnIOS11OrLater() && IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Test disabled on iOS 11.");
  }

  const GURL URL = web::test::HttpServer::MakeUrl("http://origin");

  [ChromeEarlGrey loadURL:URL];
  if (IsRefreshLocationBarEnabled()) {
    [[EarlGrey
        selectElementWithMatcher:chrome_test_util::DefocusedLocationView()]
        performAction:grey_tap()];
  }

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"foo")];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText("foo")];

  id<GREYMatcher> cancelButton = grey_accessibilityLabel(@"Clear Text");

  [[EarlGrey selectElementWithMatcher:cancelButton] performAction:grey_tap()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText("")];
}

// Types JavaScript into Omnibox and verify that an alert is displayed.
- (void)testTypeJavaScriptIntoOmnibox {
  // TODO(crbug.com/642544): Enable the test for iPad when typing bug is fixed.
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to a typing bug.");
  }

  std::map<GURL, std::string> responses;
  GURL URL = web::test::HttpServer::MakeUrl("http://foo");
  responses[URL] = "bar";
  web::test::SetUpSimpleHttpServer(responses);
  [ChromeEarlGrey loadURL:GURL(URL)];

  [ChromeEarlGreyUI focusOmniboxAndType:@"javascript:alert('Hello');\n"];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Hello")]
      assertWithMatcher:grey_notNil()];
}

// Loads WebUI page, types JavaScript into Omnibox and verifies that alert is
// not displayed. WebUI pages have elevated privileges and should not allow
// script execution.
- (void)testTypeJavaScriptIntoOmniboxWithWebUIPage {
  // TODO(crbug.com/642544): Enable the test for iPad when typing bug is fixed.
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to a typing bug.");
  }
  [ChromeEarlGrey loadURL:GURL("chrome://version")];
  [ChromeEarlGreyUI focusOmniboxAndType:@"javascript:alert('Hello');\n"];

  [[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(@"Hello")]
      assertWithMatcher:grey_nil()];
}

// Tests typing in the omnibox.
- (void)testToolbarOmniboxTyping {
  // TODO(crbug.com/642559): Enable this test for iPad when typing bug is fixed.
  if (IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Disabled for iPad due to a simulator bug.");
  }

  [[EarlGrey selectElementWithMatcher:chrome_test_util::NewTabPageOmnibox()]
      performAction:grey_typeText(@"a")];
  [[EarlGrey selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(@"a"),
                                                 grey_kindOfClass(
                                                     [OmniboxPopupRow class]),
                                                 nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"b")];
  [[EarlGrey selectElementWithMatcher:grey_allOf(grey_accessibilityLabel(@"ab"),
                                                 grey_kindOfClass(
                                                     [OmniboxPopupRow class]),
                                                 nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"C")];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(@"abC"),
                                   grey_kindOfClass([OmniboxPopupRow class]),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"1")];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(@"abC1"),
                                   grey_kindOfClass([OmniboxPopupRow class]),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"2")];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(@"abC12"),
                                   grey_kindOfClass([OmniboxPopupRow class]),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"@")];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(@"abC12@"),
                                   grey_kindOfClass([OmniboxPopupRow class]),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"{")];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(@"abC12@{"),
                                   grey_kindOfClass([OmniboxPopupRow class]),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      performAction:grey_typeText(@"#")];
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(@"abC12@{#"),
                                   grey_kindOfClass([OmniboxPopupRow class]),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];

  id<GREYMatcher> cancelButton = nil;
  if (IsUIRefreshPhase1Enabled()) {
    cancelButton =
        grey_accessibilityID(kToolbarCancelOmniboxEditButtonIdentifier);
  } else if (IsIPadIdiom()) {
    NSString* typingShield = @"Hide keyboard";
    cancelButton = grey_accessibilityLabel(typingShield);
  } else {
    NSString* cancelText = l10n_util::GetNSString(IDS_CANCEL);
    cancelButton = grey_accessibilityLabel(cancelText);
  }
  DCHECK(cancelButton);

  [[EarlGrey
      selectElementWithMatcher:grey_allOf(cancelButton,
                                          grey_sufficientlyVisible(), nil)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::Omnibox()]
      assertWithMatcher:chrome_test_util::OmniboxText("")];
}

// Tests typing in the omnibox using the keyboard accessory view.
- (void)testToolbarOmniboxKeyboardAccessoryView {
  // Select the omnibox to get the keyboard up.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::NewTabPageOmnibox()]
      performAction:grey_tap()];
  [ChromeEarlGrey
      waitForElementWithMatcherSufficientlyVisible:chrome_test_util::Omnibox()];

  // Tap the "/" keyboard accessory button.
  id<GREYMatcher> slashButtonMatcher = grey_allOf(
      grey_accessibilityLabel(@"/"), grey_kindOfClass([UIButton class]), nil);

  [[EarlGrey selectElementWithMatcher:slashButtonMatcher]
      performAction:grey_tap()];

  // Tap the ".com" keyboard accessory button.
  id<GREYMatcher> dotComButtonMatcher =
      grey_allOf(grey_accessibilityLabel(@".com"),
                 grey_kindOfClass([UIButton class]), nil);

  [[EarlGrey selectElementWithMatcher:dotComButtonMatcher]
      performAction:grey_tap()];

  // Verify that the omnibox contains "/.com"
  [[EarlGrey
      selectElementWithMatcher:grey_allOf(
                                   grey_accessibilityLabel(@"/.com"),
                                   grey_kindOfClass([OmniboxPopupRow class]),
                                   nil)]
      assertWithMatcher:grey_sufficientlyVisible()];
}

@end

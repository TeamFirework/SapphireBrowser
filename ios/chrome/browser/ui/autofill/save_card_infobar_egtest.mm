// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/logging.h"
#import "base/test/ios/wait_util.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/ios/browser/autofill_driver_ios.h"
#import "components/autofill/ios/browser/credit_card_save_manager_test_observer_bridge.h"
#include "components/autofill/ios/browser/ios_test_event_waiter.h"
#import "ios/chrome/browser/ui/autofill/save_card_infobar_controller.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server/http_server.h"
#include "ios/web/public/web_state/web_frame_util.h"
#import "ios/web/public/web_state/web_frames_manager.h"
#import "ios/web/public/web_state/web_state.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

using base::test::ios::kWaitForDownloadTimeout;
using base::test::ios::kWaitForUIElementTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

// URLs of the test pages.
const char kCreditCardUploadForm[] =
    "https://components/test/data/autofill/"
    "credit_card_upload_form_address_and_cc.html";

// Google Payments server requests and responses.
const char kURLGetUploadDetailsRequest[] =
    "https://payments.google.com/payments/apis/chromepaymentsservice/"
    "getdetailsforsavecard";
const char kResponseGetUploadDetailsSuccess[] =
    "{\"legal_message\":{\"line\":[{\"template\":\"Legal message template with "
    "link: "
    "{0}.\",\"template_parameter\":[{\"display_text\":\"Link\",\"url\":\"https:"
    "//www.example.com/\"}]}]},\"context_token\":\"dummy_context_token\"}";
const char kResponseGetUploadDetailsFailure[] =
    "{\"error\":{\"code\":\"FAILED_PRECONDITION\",\"user_error_message\":\"An "
    "unexpected error has occurred. Please try again later.\"}}";

// CreditCardSaveManager events that can be waited on by the IOSTestEventWaiter.
enum InfobarEvent : int {
  OFFERED_LOCAL_SAVE,
  REQUESTED_UPLOAD_SAVE,
  RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
  SENT_UPLOAD_CARD_REQUEST,
};

}  // namepsace

namespace autofill {

// Helper class that provides access to private members of AutofillManager and
// FormDataImporter.
class SaveCardInfobarEGTestHelper {
 public:
  SaveCardInfobarEGTestHelper() {}
  ~SaveCardInfobarEGTestHelper() {}

  static CreditCardSaveManager* credit_card_save_manager() {
    web::WebState* web_state = chrome_test_util::GetCurrentWebState();
    web::WebFrame* main_frame = web::GetMainWebFrame(web_state);
    DCHECK(web_state);
    return AutofillDriverIOS::FromWebStateAndWebFrame(web_state, main_frame)
        ->autofill_manager()
        ->form_data_importer_.get()
        ->credit_card_save_manager_.get();
  }

  static payments::PaymentsClient* payments_client() {
    web::WebState* web_state = chrome_test_util::GetCurrentWebState();
    web::WebFrame* main_frame = web::GetMainWebFrame(web_state);
    DCHECK(web_state);
    return AutofillDriverIOS::FromWebStateAndWebFrame(web_state, main_frame)
        ->autofill_manager()
        ->payments_client();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SaveCardInfobarEGTestHelper);
};

}  // namespace autofill

@interface SaveCardInfobarEGTest
    : ChromeTestCase<CreditCardSaveManagerTestObserver> {
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  std::unique_ptr<autofill::CreditCardSaveManagerTestObserverBridge>
      credit_card_save_manager_observer_;
  std::unique_ptr<autofill::IOSTestEventWaiter<InfobarEvent>> event_waiter_;
}

@end

@implementation SaveCardInfobarEGTest

- (void)setUp {
  [super setUp];

  // Set up the URL loader factory for the PaymentsClient so we can intercept
  // those network requests.
  shared_url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  autofill::payments::PaymentsClient* payments_client =
      autofill::SaveCardInfobarEGTestHelper::payments_client();
  payments_client->set_url_loader_factory_for_testing(
      shared_url_loader_factory_);

  // Observe actions in CreditCardSaveManager.
  autofill::CreditCardSaveManager* credit_card_save_manager =
      autofill::SaveCardInfobarEGTestHelper::credit_card_save_manager();
  credit_card_save_manager_observer_ =
      std::make_unique<autofill::CreditCardSaveManagerTestObserverBridge>(
          credit_card_save_manager, self);
}

#pragma mark - autofill::IOSTestEventWaiter helper methods

- (void)resetEventWaiterForEvents:(std::list<InfobarEvent>)events
                          timeout:(NSTimeInterval)timeout {
  event_waiter_ = std::make_unique<autofill::IOSTestEventWaiter<InfobarEvent>>(
      std::move(events), timeout);
}

- (void)onEvent:(InfobarEvent)event {
  GREYAssertTrue(event_waiter_->OnEvent(event),
                 @"Unexpected event was observed.");
}

- (void)waitForEvents {
  GREYAssertTrue(event_waiter_->Wait(),
                 @"One or more events were not observed.");
}

#pragma mark - CreditCardSaveManagerTestObserver

- (void)offeredLocalSave {
  [self onEvent:InfobarEvent::OFFERED_LOCAL_SAVE];
}

- (void)decidedToRequestUploadSave {
  [self onEvent:InfobarEvent::REQUESTED_UPLOAD_SAVE];
}

- (void)receivedGetUploadDetailsResponse {
  [self onEvent:InfobarEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE];
}

- (void)sentUploadCardRequest {
  [self onEvent:InfobarEvent::SENT_UPLOAD_CARD_REQUEST];
}

#pragma mark - Page interaction helper methods

- (void)fillAndSubmitFormWithCardDetailsOnly {
  NSError* error = nil;
  chrome_test_util::ExecuteJavaScript(
      @"(function() { document.getElementById('fill_card_only').click(); })();",
      &error);
  GREYAssert(!error, @"Error during script execution: %@", error);

  [self submitForm];
}

- (void)fillAndSubmitForm {
  NSError* error = nil;
  chrome_test_util::ExecuteJavaScript(
      @"(function() { document.getElementById('fill_form').click(); })();",
      &error);
  GREYAssert(!error, @"Error during script execution: %@", error);

  [self submitForm];
}

- (void)submitForm {
  NSError* error = nil;
  chrome_test_util::ExecuteJavaScript(
      @"(function() { document.getElementById('submit').click(); })();",
      &error);
  GREYAssert(!error, @"Error during script execution: %@", error);
}

#pragma mark - Helper methods

- (BOOL)waitForSaveCardInfobarOrTimeout:(NSString*)accessibilityIdentifier {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityID(accessibilityIdentifier)]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return error == nil;
  };
  return WaitUntilConditionOrTimeout(kWaitForUIElementTimeout, condition);
}

#pragma mark - Tests

// Ensures that submitting the form should query Google Payments; and the
// fallback local save infobar appears if the request unexpectedly fails but the
// form data is complete.
- (void)testOfferLocalSave_FullData_RequestFails {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Set up the Google Payments server response.
  test_url_loader_factory_.AddResponse(kURLGetUploadDetailsRequest,
                                       kResponseGetUploadDetailsSuccess,
                                       net::HTTP_INTERNAL_SERVER_ERROR);

  [self resetEventWaiterForEvents:
            {InfobarEvent::REQUESTED_UPLOAD_SAVE,
             InfobarEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
             InfobarEvent::OFFERED_LOCAL_SAVE}
                          timeout:kWaitForDownloadTimeout];
  [self fillAndSubmitForm];
  [self waitForEvents];

  // Wait until the save card infobar becomes visible.
  GREYAssert([self waitForSaveCardInfobarOrTimeout:
                       kSaveCardInfobarViewLocalAccessibilityID],
             @"Save card infobar failed to show.");
}

// Ensures that submitting the form should query Google Payments; and the
// fallback local save infobar appears if the request is declined but the form
// data is complete.
- (void)testOfferLocalSave_FullData_PaymentsDeclines {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Set up the Google Payments server response.
  test_url_loader_factory_.AddResponse(kURLGetUploadDetailsRequest,
                                       kResponseGetUploadDetailsFailure);

  [self resetEventWaiterForEvents:
            {InfobarEvent::REQUESTED_UPLOAD_SAVE,
             InfobarEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE,
             InfobarEvent::OFFERED_LOCAL_SAVE}
                          timeout:kWaitForDownloadTimeout];
  [self fillAndSubmitForm];
  [self waitForEvents];

  // Wait until the save card infobar becomes visible.
  GREYAssert([self waitForSaveCardInfobarOrTimeout:
                       kSaveCardInfobarViewLocalAccessibilityID],
             @"Save card infobar failed to show.");
}

// Ensures that submitting the form, even with only card number and expiration
// date, should query Google Payments; but the fallback local save infobar
// should not appear if the request is declined and the form data is incomplete.
- (void)testNoLocalSave_PartialData_PaymentsDeclines {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Set up the Google Payments server response.
  test_url_loader_factory_.AddResponse(kURLGetUploadDetailsRequest,
                                       kResponseGetUploadDetailsFailure);

  [self resetEventWaiterForEvents:
            {InfobarEvent::REQUESTED_UPLOAD_SAVE,
             InfobarEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE}
                          timeout:kWaitForDownloadTimeout];
  [self fillAndSubmitFormWithCardDetailsOnly];
  [self waitForEvents];

  // Make sure the save card infobar does not become visible.
  GREYAssertFalse([self waitForSaveCardInfobarOrTimeout:
                            kSaveCardInfobarViewLocalAccessibilityID],
                  @"Save card infobar should not show.");
}

// Ensures that submitting the form should query Google Payments; and the
// upstreaming infobar should appear if the request is accepted.
- (void)testOfferUpstream_FullData_PaymentsAccepts {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Set up the Google Payments server response.
  test_url_loader_factory_.AddResponse(kURLGetUploadDetailsRequest,
                                       kResponseGetUploadDetailsSuccess);

  [self resetEventWaiterForEvents:
            {InfobarEvent::REQUESTED_UPLOAD_SAVE,
             InfobarEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE}
                          timeout:kWaitForDownloadTimeout];
  [self fillAndSubmitForm];
  [self waitForEvents];

  // Wait until the save card infobar becomes visible.
  GREYAssert([self waitForSaveCardInfobarOrTimeout:
                       kSaveCardInfobarViewUploadAccessibilityID],
             @"Save card infobar failed to show.");
}

// Ensures that submitting the form, even with only card number and expiration
// date, should query Google Payments and the upstreaming infobar should appear
// if the request is accepted.
- (void)testOfferUpstream_PartialData_PaymentsAccepts {
  [ChromeEarlGrey
      loadURL:web::test::HttpServer::MakeUrl(kCreditCardUploadForm)];

  // Set up the Google Payments server response.
  test_url_loader_factory_.AddResponse(kURLGetUploadDetailsRequest,
                                       kResponseGetUploadDetailsSuccess);

  [self resetEventWaiterForEvents:
            {InfobarEvent::REQUESTED_UPLOAD_SAVE,
             InfobarEvent::RECEIVED_GET_UPLOAD_DETAILS_RESPONSE}
                          timeout:kWaitForDownloadTimeout];
  [self fillAndSubmitFormWithCardDetailsOnly];
  [self waitForEvents];

  // Wait until the save card infobar becomes visible.
  GREYAssert([self waitForSaveCardInfobarOrTimeout:
                       kSaveCardInfobarViewUploadAccessibilityID],
             @"Save card infobar failed to show.");
}

@end

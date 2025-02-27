// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/webstore_widget_private/app_installer.h"

#include "base/macros.h"
#include "chrome/common/extensions/webstore_install_result.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

namespace {
const char kWebContentsDestroyedError[] = "WebContents is destroyed.";
}  // namespace

namespace webstore_widget {

class AppInstaller::WebContentsObserver : public content::WebContentsObserver {
 public:
  WebContentsObserver(content::WebContents* web_contents, AppInstaller* parent)
      : content::WebContentsObserver(web_contents), parent_(parent) {}

 protected:
  // content::WebContentsObserver implementation.
  void WebContentsDestroyed() override {
    parent_->OnWebContentsDestroyed(web_contents());
  }

 private:
  AppInstaller* parent_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(WebContentsObserver);
};

AppInstaller::AppInstaller(content::WebContents* web_contents,
                           const std::string& item_id,
                           Profile* profile,
                           bool silent_installation,
                           const Callback& callback)
    : extensions::WebstoreStandaloneInstaller(item_id, profile, callback),
      silent_installation_(silent_installation),
      callback_(callback),
      web_contents_(web_contents),
      web_contents_observer_(new WebContentsObserver(web_contents, this)) {
  DCHECK(web_contents_);
}

AppInstaller::~AppInstaller() {
}

bool AppInstaller::CheckRequestorAlive() const {
  // The tab may have gone away - cancel installation in that case.
  return web_contents_ != NULL;
}

const GURL& AppInstaller::GetRequestorURL() const {
  return GURL::EmptyGURL();
}

std::unique_ptr<ExtensionInstallPrompt::Prompt>
AppInstaller::CreateInstallPrompt() const {
  if (silent_installation_)
    return nullptr;

  std::unique_ptr<ExtensionInstallPrompt::Prompt> prompt(
      new ExtensionInstallPrompt::Prompt(
          ExtensionInstallPrompt::INLINE_INSTALL_PROMPT));

  prompt->SetWebstoreData(localized_user_count(), show_user_count(),
                          average_rating(), rating_count());
  return prompt;
}

bool AppInstaller::ShouldShowPostInstallUI() const {
  return false;
}

bool AppInstaller::ShouldShowAppInstalledBubble() const {
  return false;
}

content::WebContents* AppInstaller::GetWebContents() const {
  return web_contents_;
}

bool AppInstaller::CheckInlineInstallPermitted(
    const base::DictionaryValue& webstore_data,
    std::string* error) const {
  DCHECK(error != NULL);
  DCHECK(error->empty());

  // We expect to be able to inline install the app.
  bool inline_install_not_supported = false;
  if (webstore_data.HasKey(kInlineInstallNotSupportedKey) &&
      !webstore_data.GetBoolean(kInlineInstallNotSupportedKey,
                                &inline_install_not_supported)) {
    *error = extensions::webstore_install::kInvalidWebstoreResponseError;
    return false;
  }

  DCHECK(!inline_install_not_supported)
      << "App does not support inline installation";

  if (inline_install_not_supported) {
    *error = extensions::webstore_install::kInvalidWebstoreResponseError;
    return false;
  }

  return true;
}

bool AppInstaller::CheckRequestorPermitted(
    const base::DictionaryValue& webstore_data,
    std::string* error) const {
  DCHECK(error != NULL);
  DCHECK(error->empty());
  return true;
}

void AppInstaller::OnWebContentsDestroyed(content::WebContents* web_contents) {
  callback_.Run(false, kWebContentsDestroyedError,
                extensions::webstore_install::OTHER_ERROR);
  AbortInstall();
}

}  // namespace webstore_widget

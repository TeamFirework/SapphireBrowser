// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_interaction_controller.h"

#include <stdint.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/experimental_flags.h"
#import "ios/chrome/browser/metrics/new_tab_page_uma.h"
#import "ios/chrome/browser/tabs/tab.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_edit_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_folder_editor_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_home_view_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_interaction_controller_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_mediator.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_path_cache.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_transitioning_delegate.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_navigation_controller_delegate.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller.h"
#import "ios/chrome/browser/ui/table_view/table_view_presentation_controller_delegate.h"
#include "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/browser/ui/url_loader.h"
#import "ios/chrome/browser/ui/util/form_sheet_navigation_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#import "ios/web/public/navigation_manager.h"
#include "ios/web/public/referrer.h"
#import "ios/web/public/web_state/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

namespace {

// Tracks the type of UI that is currently being presented.
enum class PresentedState {
  NONE,
  BOOKMARK_BROWSER,
  BOOKMARK_EDITOR,
  FOLDER_EDITOR,
};

}  // namespace

@interface BookmarkInteractionController ()<
    BookmarkEditViewControllerDelegate,
    BookmarkFolderEditorViewControllerDelegate,
    BookmarkHomeViewControllerDelegate,
    TableViewPresentationControllerDelegate> {
  // The browser state of the current user.
  ios::ChromeBrowserState* _currentBrowserState;  // weak

  // The browser state to use, might be different from _currentBrowserState if
  // it is incognito.
  ios::ChromeBrowserState* _browserState;  // weak

  // The designated url loader.
  __weak id<UrlLoader> _loader;

  // The parent controller on top of which the UI needs to be presented.
  __weak UIViewController* _parentController;
}

// The type of view controller that is being presented.
@property(nonatomic, assign) PresentedState currentPresentedState;

// The navigation controller that is being presented, if any.
// |self.bookmarkBrowser|, |self.bookmarkEditor|, and |self.folderEditor| are
// children of this navigation controller.
@property(nonatomic, strong)
    UINavigationController* bookmarkNavigationController;

// The delegate provided to |self.bookmarkNavigationController|.
@property(nonatomic, strong)
    TableViewNavigationControllerDelegate* bookmarkNavigationControllerDelegate;

// The bookmark model in use.
@property(nonatomic, assign) BookmarkModel* bookmarkModel;

// A reference to the potentially presented bookmark browser. This will be
// non-nil when |currentPresentedState| is BOOKMARK_BROWSER.
@property(nonatomic, strong) BookmarkHomeViewController* bookmarkBrowser;

// A reference to the potentially presented single bookmark editor. This will be
// non-nil when |currentPresentedState| is BOOKMARK_EDITOR.
@property(nonatomic, strong) BookmarkEditViewController* bookmarkEditor;

// A reference to the potentially presented folder editor. This will be non-nil
// when |currentPresentedState| is FOLDER_EDITOR.
@property(nonatomic, strong) BookmarkFolderEditorViewController* folderEditor;

@property(nonatomic, strong) BookmarkMediator* mediator;

@property(nonatomic, readonly, weak) id<ApplicationCommands> dispatcher;

// The transitioning delegate that is used when presenting
// |self.bookmarkBrowser|.
@property(nonatomic, strong)
    BookmarkTransitioningDelegate* bookmarkTransitioningDelegate;

// Builds a controller and brings it on screen.
- (void)presentBookmarkEditorForBookmarkedTab:(Tab*)tab;

// Dismisses the bookmark browser.  If |urlsToOpen| is not empty, then the user
// has selected to navigate to those URLs with specified tab mode.
- (void)dismissBookmarkBrowserAnimated:(BOOL)animated
                            urlsToOpen:(const std::vector<GURL>&)urlsToOpen
                           inIncognito:(BOOL)inIncognito
                                newTab:(BOOL)newTab;

// Dismisses the bookmark editor.
- (void)dismissBookmarkEditorAnimated:(BOOL)animated;

// Dismisses the folder editor.
- (void)dismissFolderEditorAnimated:(BOOL)animated;

@end

@implementation BookmarkInteractionController
@synthesize bookmarkBrowser = _bookmarkBrowser;
@synthesize bookmarkEditor = _bookmarkEditor;
@synthesize bookmarkModel = _bookmarkModel;
@synthesize bookmarkNavigationController = _bookmarkNavigationController;
@synthesize bookmarkNavigationControllerDelegate =
    _bookmarkNavigationControllerDelegate;
@synthesize bookmarkTransitioningDelegate = _bookmarkTransitioningDelegate;
@synthesize currentPresentedState = _currentPresentedState;
@synthesize delegate = _delegate;
@synthesize dispatcher = _dispatcher;
@synthesize folderEditor = _folderEditor;
@synthesize mediator = _mediator;

- (instancetype)initWithBrowserState:(ios::ChromeBrowserState*)browserState
                              loader:(id<UrlLoader>)loader
                    parentController:(UIViewController*)parentController
                          dispatcher:(id<ApplicationCommands>)dispatcher {
  self = [super init];
  if (self) {
    // Bookmarks are always opened with the main browser state, even in
    // incognito mode.
    _currentBrowserState = browserState;
    _browserState = browserState->GetOriginalChromeBrowserState();
    _loader = loader;
    _parentController = parentController;
    _dispatcher = dispatcher;
    _bookmarkModel =
        ios::BookmarkModelFactory::GetForBrowserState(_browserState);
    _mediator = [[BookmarkMediator alloc] initWithBrowserState:_browserState];
    _currentPresentedState = PresentedState::NONE;
    DCHECK(_bookmarkModel);
    DCHECK(_parentController);
  }
  return self;
}

- (void)dealloc {
  _bookmarkBrowser.homeDelegate = nil;
  _bookmarkEditor.delegate = nil;
}

- (void)presentBookmarkEditorForBookmarkedTab:(Tab*)tab {
  DCHECK(tab && tab.webState);

  const BookmarkNode* bookmark =
      self.bookmarkModel->GetMostRecentlyAddedUserNodeForURL(
          tab.webState->GetLastCommittedURL());
  if (!bookmark)
    return;
  [self presentEditorForNode:bookmark];
}

- (void)presentBookmarkEditorForTab:(Tab*)tab
                currentlyBookmarked:(BOOL)bookmarked {
  if (!self.bookmarkModel->loaded())
    return;
  if (!tab || !tab.webState)
    return;

  if (bookmarked) {
    [self presentBookmarkEditorForBookmarkedTab:tab];
  } else {
    __weak BookmarkInteractionController* weakSelf = self;
    __weak Tab* weakTab = tab;
    void (^editAction)() = ^{
      BookmarkInteractionController* strongSelf = weakSelf;
      if (!strongSelf || !weakTab || !weakTab.webState)
        return;
      [strongSelf presentBookmarkEditorForBookmarkedTab:weakTab];
    };
    [self.mediator addBookmarkWithTitle:tab.title
                                    URL:tab.webState->GetLastCommittedURL()
                             editAction:editAction];
  }
}

- (void)presentBookmarks {
  DCHECK_EQ(PresentedState::NONE, self.currentPresentedState);
  DCHECK(!self.bookmarkNavigationController);

  self.bookmarkBrowser =
      [[BookmarkHomeViewController alloc] initWithLoader:_loader
                                            browserState:_currentBrowserState
                                              dispatcher:self.dispatcher];
  self.bookmarkBrowser.homeDelegate = self;

  NSArray<BookmarkHomeViewController*>* replacementViewControllers = nil;
  if (self.bookmarkModel->loaded()) {
    // Set the root node if the model has been loaded. If the model has not been
    // loaded yet, the root node will be set in BookmarkHomeViewController after
    // the model is finished loading.
    [self.bookmarkBrowser setRootNode:self.bookmarkModel->root_node()];
    replacementViewControllers =
        [self.bookmarkBrowser cachedViewControllerStack];
  }

  [self presentTableViewController:self.bookmarkBrowser
      withReplacementViewControllers:replacementViewControllers];
  self.currentPresentedState = PresentedState::BOOKMARK_BROWSER;
}

- (void)presentEditorForNode:(const bookmarks::BookmarkNode*)node {
  DCHECK_EQ(PresentedState::NONE, self.currentPresentedState);

  [self dismissSnackbar];

  if (!node) {
    return;
  }

  if (!(node->type() == BookmarkNode::URL ||
        node->type() == BookmarkNode::FOLDER)) {
    return;
  }

  ChromeTableViewController* editorController = nil;
  if (node->type() == BookmarkNode::URL) {
    self.currentPresentedState = PresentedState::BOOKMARK_EDITOR;
    BookmarkEditViewController* bookmarkEditor =
        [[BookmarkEditViewController alloc] initWithBookmark:node
                                                browserState:_browserState];
    self.bookmarkEditor = bookmarkEditor;
    self.bookmarkEditor.delegate = self;
    editorController = bookmarkEditor;
  } else if (node->type() == BookmarkNode::FOLDER) {
    self.currentPresentedState = PresentedState::FOLDER_EDITOR;
    BookmarkFolderEditorViewController* folderEditor =
        [BookmarkFolderEditorViewController
            folderEditorWithBookmarkModel:self.bookmarkModel
                                   folder:node
                             browserState:_browserState];
    folderEditor.delegate = self;
    self.folderEditor = folderEditor;
    editorController = folderEditor;
  } else {
    NOTREACHED();
  }

  [self presentTableViewController:editorController
      withReplacementViewControllers:nil];
}

- (void)dismissBookmarkBrowserAnimated:(BOOL)animated
                            urlsToOpen:(const std::vector<GURL>&)urlsToOpen
                           inIncognito:(BOOL)inIncognito
                                newTab:(BOOL)newTab {
  if (self.currentPresentedState != PresentedState::BOOKMARK_BROWSER)
    return;
  DCHECK(self.bookmarkNavigationController);

  // If trying to open urls with tab mode changed, we need to postpone openUrls
  // until the dismissal of Bookmarks is done.  This is to prevent the race
  // condition between the dismissal of bookmarks and switch of BVC.
  const BOOL openUrlsAfterDismissal =
      !urlsToOpen.empty() &&
      ((!!inIncognito) != _currentBrowserState->IsOffTheRecord());

  // A copy of the urls vector for the completion block.
  std::vector<GURL> urlsToOpenAfterDismissal;
  if (openUrlsAfterDismissal) {
    // open urls in the completion block after dismissal.
    urlsToOpenAfterDismissal = urlsToOpen;
  } else if (!urlsToOpen.empty()) {
    // open urls now.
    [self openUrls:urlsToOpen inIncognito:inIncognito newTab:newTab];
  }

  [_parentController
      dismissViewControllerAnimated:animated
                         completion:^{
                           self.bookmarkBrowser.homeDelegate = nil;
                           self.bookmarkBrowser = nil;
                           self.bookmarkNavigationController = nil;
                           self.bookmarkTransitioningDelegate = nil;
                           self.bookmarkNavigationController = nil;
                           self.bookmarkNavigationControllerDelegate = nil;

                           if (!openUrlsAfterDismissal) {
                             return;
                           }
                           [self openUrls:urlsToOpenAfterDismissal
                               inIncognito:inIncognito
                                    newTab:newTab];
                         }];
  self.currentPresentedState = PresentedState::NONE;
}

- (void)dismissBookmarkEditorAnimated:(BOOL)animated {
  if (self.currentPresentedState != PresentedState::BOOKMARK_EDITOR)
    return;
  DCHECK(self.bookmarkNavigationController);

  self.bookmarkEditor.delegate = nil;
  self.bookmarkEditor = nil;
  [self.bookmarkNavigationController
      dismissViewControllerAnimated:animated
                         completion:^{
                           self.bookmarkNavigationController = nil;
                           self.bookmarkTransitioningDelegate = nil;
                         }];
  self.currentPresentedState = PresentedState::NONE;
}

- (void)dismissFolderEditorAnimated:(BOOL)animated {
  if (self.currentPresentedState != PresentedState::FOLDER_EDITOR)
    return;
  DCHECK(self.bookmarkNavigationController);

  [self.bookmarkNavigationController
      dismissViewControllerAnimated:animated
                         completion:^{
                           self.folderEditor.delegate = nil;
                           self.folderEditor = nil;
                           self.bookmarkNavigationController = nil;
                           self.bookmarkTransitioningDelegate = nil;
                         }];
  self.currentPresentedState = PresentedState::NONE;
}

- (void)dismissBookmarkModalControllerAnimated:(BOOL)animated {
  // No urls to open.  So it does not care about inIncognito and newTab.
  [self dismissBookmarkBrowserAnimated:animated
                            urlsToOpen:std::vector<GURL>()
                           inIncognito:NO
                                newTab:NO];
  [self dismissBookmarkEditorAnimated:animated];
}

- (void)dismissSnackbar {
  // Dismiss any bookmark related snackbar this controller could have presented.
  [MDCSnackbarManager dismissAndCallCompletionBlocksWithCategory:
                          bookmark_utils_ios::kBookmarksSnackbarCategory];
}

#pragma mark - BookmarkEditViewControllerDelegate

- (BOOL)bookmarkEditor:(BookmarkEditViewController*)controller
    shoudDeleteAllOccurencesOfBookmark:(const BookmarkNode*)bookmark {
  return YES;
}

- (void)bookmarkEditorWantsDismissal:(BookmarkEditViewController*)controller {
  [self dismissBookmarkEditorAnimated:YES];
}

- (void)bookmarkEditorWillCommitTitleOrUrlChange:
    (BookmarkEditViewController*)controller {
  [self.delegate bookmarkInteractionControllerWillCommitTitleOrUrlChange:self];
}

#pragma mark - BookmarkFolderEditorViewControllerDelegate

- (void)bookmarkFolderEditor:(BookmarkFolderEditorViewController*)folderEditor
      didFinishEditingFolder:(const BookmarkNode*)folder {
  DCHECK(folder);
  [self dismissFolderEditorAnimated:YES];
}

- (void)bookmarkFolderEditorDidDeleteEditedFolder:
    (BookmarkFolderEditorViewController*)folderEditor {
  [self dismissFolderEditorAnimated:YES];
}

- (void)bookmarkFolderEditorDidCancel:
    (BookmarkFolderEditorViewController*)folderEditor {
  [self dismissFolderEditorAnimated:YES];
}

- (void)bookmarkFolderEditorWillCommitTitleChange:
    (BookmarkFolderEditorViewController*)controller {
  [self.delegate bookmarkInteractionControllerWillCommitTitleOrUrlChange:self];
}

#pragma mark - BookmarkHomeViewControllerDelegate

- (void)
bookmarkHomeViewControllerWantsDismissal:(BookmarkHomeViewController*)controller
                        navigationToUrls:(const std::vector<GURL>&)urls {
  [self bookmarkHomeViewControllerWantsDismissal:controller
                                navigationToUrls:urls
                                     inIncognito:_currentBrowserState
                                                     ->IsOffTheRecord()
                                          newTab:NO];
}

- (void)bookmarkHomeViewControllerWantsDismissal:
            (BookmarkHomeViewController*)controller
                                navigationToUrls:(const std::vector<GURL>&)urls
                                     inIncognito:(BOOL)inIncognito
                                          newTab:(BOOL)newTab {
  [self dismissBookmarkBrowserAnimated:YES
                            urlsToOpen:urls
                           inIncognito:inIncognito
                                newTab:newTab];
}

- (void)openUrls:(const std::vector<GURL>&)urls
     inIncognito:(BOOL)inIncognito
          newTab:(BOOL)newTab {
  BOOL openInForegroundTab = YES;
  for (const GURL& url : urls) {
    DCHECK(url.is_valid());
    // TODO(crbug.com/695749): Force url to open in non-incognito mode. if
    // !IsURLAllowedInIncognito(url).

    if (openInForegroundTab) {
      // Only open the first URL in foreground tab.
      openInForegroundTab = NO;

      // TODO(crbug.com/695749): See if we need different metrics for 'Open
      // all', 'Open all in incognito' and 'Open in incognito'.
      new_tab_page_uma::RecordAction(_browserState,
                                     new_tab_page_uma::ACTION_OPENED_BOOKMARK);
      base::RecordAction(
          base::UserMetricsAction("MobileBookmarkManagerEntryOpened"));

      if (newTab ||
          ((!!inIncognito) != _currentBrowserState->IsOffTheRecord())) {
        // Open in new tab if it is specified or target tab mode is different
        // from current tab mode.
        [self openURLInNewTab:url inIncognito:inIncognito inBackground:NO];
      } else {
        // Open in current tab otherwise.
        [self openURLInCurrentTab:url];
      }
    } else {
      // Open other URLs (if any) in background tabs.
      [self openURLInNewTab:url inIncognito:inIncognito inBackground:YES];
    }
  }  // end for
}

#pragma mark - TableViewPresentationControllerDelegate

- (BOOL)presentationControllerShouldDismissOnTouchOutside:
    (TableViewPresentationController*)controller {
  BOOL shouldDismissOnTouchOutside = YES;

  ChromeTableViewController* tableViewController =
      base::mac::ObjCCast<ChromeTableViewController>(
          self.bookmarkNavigationController.topViewController);
  if (tableViewController) {
    shouldDismissOnTouchOutside =
        [tableViewController shouldBeDismissedOnTouchOutside];
  }
  return shouldDismissOnTouchOutside;
}

- (void)presentationControllerWillDismiss:
    (TableViewPresentationController*)controller {
  [self dismissBookmarkModalControllerAnimated:YES];
}

#pragma mark - Private

// Presents |viewController| using the appropriate presentation and styling,
// depending on whether the UIRefresh experiment is enabled or disabled. Sets
// |self.bookmarkNavigationController| to the UINavigationController subclass
// used, and may set |self.bookmarkNavigationControllerDelegate| or
// |self.bookmarkTransitioningDelegate| depending on whether or not the desired
// transition requires those objects.  If |replacementViewControllers| is not
// nil, those controllers are swapped in to the UINavigationController instead
// of |viewController|.
- (void)presentTableViewController:(ChromeTableViewController*)viewController
    withReplacementViewControllers:
        (NSArray<ChromeTableViewController*>*)replacementViewControllers {
  if (experimental_flags::IsBookmarksUIRebootEnabled()) {
    TableViewNavigationController* navController =
        [[TableViewNavigationController alloc] initWithTable:viewController];
    self.bookmarkNavigationController = navController;
    if (replacementViewControllers) {
      [navController setViewControllers:replacementViewControllers];
    }

    navController.toolbarHidden = YES;
    self.bookmarkNavigationControllerDelegate =
        [[TableViewNavigationControllerDelegate alloc] init];
    navController.delegate = self.bookmarkNavigationControllerDelegate;
    self.bookmarkTransitioningDelegate =
        [[BookmarkTransitioningDelegate alloc] init];
    self.bookmarkTransitioningDelegate.presentationControllerModalDelegate =
        self;
    navController.transitioningDelegate = self.bookmarkTransitioningDelegate;
    navController.modalPresentationStyle = UIModalPresentationCustom;

    [_parentController presentViewController:navController
                                    animated:YES
                                  completion:nil];

    TableViewPresentationController* presentationController =
        base::mac::ObjCCastStrict<TableViewPresentationController>(
            navController.presentationController);
    self.bookmarkNavigationControllerDelegate.modalController =
        presentationController;
  } else {
    FormSheetNavigationController* navController =
        [[FormSheetNavigationController alloc]
            initWithRootViewController:viewController];
    if (replacementViewControllers) {
      [navController setViewControllers:replacementViewControllers];
    }
    navController.modalPresentationStyle = UIModalPresentationFormSheet;
    self.bookmarkNavigationController = navController;
    [_parentController presentViewController:navController
                                    animated:YES
                                  completion:nil];
  }
}

- (void)openURLInCurrentTab:(const GURL&)url {
  if (url.SchemeIs(url::kJavaScriptScheme)) {  // bookmarklet
    NSString* jsToEval = [base::SysUTF8ToNSString(url.GetContent())
        stringByRemovingPercentEncoding];
    [_loader loadJavaScriptFromLocationBar:jsToEval];
    return;
  }
  web::NavigationManager::WebLoadParams params(url);
  params.transition_type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  [_loader loadURLWithParams:params];
}

- (void)openURLInNewTab:(const GURL&)url
            inIncognito:(BOOL)inIncognito
           inBackground:(BOOL)inBackground {
  // TODO(crbug.com/695749):  Open bookmarklet in new tab doesn't work.  See how
  // to deal with this later.
  OpenNewTabCommand* command =
      [[OpenNewTabCommand alloc] initWithURL:url
                                    referrer:web::Referrer()
                                 inIncognito:inIncognito
                                inBackground:inBackground
                                    appendTo:kLastTab];
  [_loader webPageOrderedOpen:command];
}

@end

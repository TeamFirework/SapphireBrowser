// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.language;

import android.content.Context;
import android.support.v7.widget.LinearLayoutManager;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.RecyclerView.ViewHolder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.CompoundButton;
import android.widget.TextView;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.modaldialog.ModalDialogManager;
import org.chromium.chrome.browser.modaldialog.ModalDialogView;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.languages.LanguageItem;
import org.chromium.components.language.GeoLanguageProviderBridge;

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;

/**
 * Implements a modal dialog that prompts the user about the languages they can read. Displayed
 * once at browser startup when no other promo or modals are shown.
 */
public class LanguageAskPrompt implements ModalDialogView.Controller {
    private class LanguageAskPromptRowViewHolder extends ViewHolder {
        private TextView mLanguageNameTextView;
        private TextView mNativeNameTextView;
        private CheckBox mCheckbox;
        private String mCode;
        private HashSet<String> mLanguagesUpdate;

        LanguageAskPromptRowViewHolder(View view) {
            super(view);
            mLanguageNameTextView =
                    ((TextView) itemView.findViewById(R.id.ui_language_representation));
            mNativeNameTextView =
                    ((TextView) itemView.findViewById(R.id.native_language_representation));
            mCheckbox = ((CheckBox) itemView.findViewById(R.id.language_ask_checkbox));
            mCheckbox.setOnCheckedChangeListener(new CompoundButton.OnCheckedChangeListener() {
                @Override
                public void onCheckedChanged(CompoundButton button, boolean isChecked) {
                    if (isChecked) {
                        mLanguagesUpdate.add(mCode);
                    } else {
                        mLanguagesUpdate.remove(mCode);
                    }
                }
            });
        }

        /**
         * Sets the text in the TextView children of this row to |languageName| and |nativeName|
         * respectively.
         * @param languageName The name of this row's language in the browser's locale.
         * @param nativeLanguage The name of this row's language as written in that language.
         * @param code The standard language code for this row's language.
         * @param languagesUpdate The set of language codes to use to know which languages to
         *        add/remove from the language list.
         */
        public void setLanguage(String languageName, String nativeName, String code,
                HashSet<String> languagesUpdate) {
            mLanguageNameTextView.setText(languageName);
            mNativeNameTextView.setText(nativeName);
            mCode = code;
            mLanguagesUpdate = languagesUpdate;
            mCheckbox.setChecked(mLanguagesUpdate.contains(mCode));
        }
    }

    private class LanguageItemAdapter extends RecyclerView.Adapter<LanguageAskPromptRowViewHolder> {
        private List<LanguageItem> mLanguages;
        private HashSet<String> mLanguagesUpdate;

        /**
         * @param context The context this item's views will be associated with.
         * @param languagesUpdate The set of language codes to use to know which languages to
         *        add/remove from the language list.
         */
        public LanguageItemAdapter(Context context, HashSet<String> languagesUpdate) {
            mLanguages = new ArrayList<LanguageItem>();
            mLanguagesUpdate = languagesUpdate;
        }

        @Override
        public LanguageAskPromptRowViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            View row = LayoutInflater.from(parent.getContext())
                               .inflate(R.layout.language_ask_prompt_row, parent, false);

            return new LanguageAskPromptRowViewHolder(row);
        }

        @Override
        public void onBindViewHolder(LanguageAskPromptRowViewHolder holder, int position) {
            LanguageItem lang = mLanguages.get(position);
            holder.setLanguage(lang.getDisplayName(), lang.getNativeDisplayName(), lang.getCode(),
                    mLanguagesUpdate);
        }

        @Override
        public int getItemCount() {
            return mLanguages.size();
        }

        /**
         * Sets the list of languages to |languages| and notifies the RecyclerView that the data has
         * changed.
         * @param languages The new list of languages to be displayed by the RecyclerView.
         */
        public void setLanguages(List<LanguageItem> languages) {
            mLanguages.clear();
            mLanguages.addAll(languages);
            notifyDataSetChanged();
        }
    }

    /**
     * Displays the Explicit Language Ask prompt if the experiment is enabled.
     * @param activity The current activity to display the prompt into.
     * @return Whether the prompt was shown or not.
     */
    public static boolean maybeShowLanguageAskPrompt(ChromeActivity activity) {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.EXPLICIT_LANGUAGE_ASK)) return false;
        if (PrefServiceBridge.getInstance().getExplicitLanguageAskPromptShown()) return false;

        LanguageAskPrompt prompt = new LanguageAskPrompt();
        prompt.show(activity);

        PrefServiceBridge.getInstance().setExplicitLanguageAskPromptShown(true);

        return true;
    }

    private ModalDialogManager mModalDialogManager;
    private ModalDialogView mDialog;
    private HashSet<String> mLanguagesUpdate;
    private HashSet<String> mInitialLanguages;

    public LanguageAskPrompt() {}

    /**
     * Mutates the user's accept languages pref so that it reflects which languages are in
     * mLanguagesUpdate, and by extension which languages were checked by the user in the prompt.
     */
    private void saveLanguages() {
        HashSet<String> languagesToAdd = new HashSet<String>(mLanguagesUpdate);
        languagesToAdd.removeAll(mInitialLanguages);

        for (String language : languagesToAdd) {
            PrefServiceBridge.getInstance().updateUserAcceptLanguages(language, true);
        }

        HashSet<String> languagesToRemove = new HashSet<String>(mInitialLanguages);
        languagesToRemove.removeAll(mLanguagesUpdate);

        for (String language : languagesToRemove) {
            PrefServiceBridge.getInstance().updateUserAcceptLanguages(language, false);
        }
    }

    /**
     * Displays this prompt inside the specified |activity|.
     * @param activity The current activity to display the prompt into.
     */
    public void show(ChromeActivity activity) {
        if (activity == null) return;

        List<String> userAcceptLanguagesList =
                PrefServiceBridge.getInstance().getUserLanguageCodes();
        mInitialLanguages = new HashSet<String>();
        mInitialLanguages.addAll(userAcceptLanguagesList);
        mLanguagesUpdate = new HashSet<String>(mInitialLanguages);

        ModalDialogView.Params params = new ModalDialogView.Params();
        params.title = activity.getString(R.string.languages_explicit_ask_title);
        params.positiveButtonTextId = R.string.save;
        params.negativeButtonTextId = R.string.cancel;
        params.cancelOnTouchOutside = true;

        RecyclerView list = new RecyclerView(activity);
        LanguageItemAdapter adapter = new LanguageItemAdapter(activity, mLanguagesUpdate);
        list.setAdapter(adapter);
        LinearLayoutManager linearLayoutManager = new LinearLayoutManager(activity);
        linearLayoutManager.setOrientation(LinearLayoutManager.VERTICAL);
        list.setLayoutManager(linearLayoutManager);
        list.setHasFixedSize(true);
        params.customView = list;

        List<LanguageItem> languages = PrefServiceBridge.getInstance().getChromeLanguageList();
        LinkedHashSet<String> currentGeoLanguages =
                GeoLanguageProviderBridge.getCurrentGeoLanguages();
        Collections.sort(languages, new Comparator<LanguageItem>() {
            private int computeItemScore(LanguageItem item) {
                // Order languages so that the region's languages are on top, followed by the ones
                // already in the user's accept languages, then the remaining languages in
                // alphabetical order.
                if (currentGeoLanguages.contains(item.getCode())) return -2;
                return mInitialLanguages.contains(item.getCode()) ? -1 : 0;
            }
            @Override
            public int compare(LanguageItem first, LanguageItem second) {
                return computeItemScore(first) - computeItemScore(second);
            }
        });
        adapter.setLanguages(languages);

        mModalDialogManager = activity.getModalDialogManager();
        mDialog = new ModalDialogView(this, params);
        mModalDialogManager.showDialog(mDialog, ModalDialogManager.ModalDialogType.APP);
    }

    @Override
    public void onClick(int buttonType) {
        if (buttonType == ModalDialogView.ButtonType.NEGATIVE) {
            mModalDialogManager.cancelDialog(mDialog);
        } else {
            saveLanguages();
            mModalDialogManager.dismissDialog(mDialog);
        }
    }

    @Override
    public void onCancel() {}

    @Override
    public void onDismiss() {}
}

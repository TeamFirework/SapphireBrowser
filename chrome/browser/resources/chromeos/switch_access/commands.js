// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class to run and get details about user commands.
 */
class Commands {
  /**
   * @constructor
   * @param {SwitchAccessInterface} switchAccess
   */
  constructor(switchAccess) {
    /**
     * SwitchAccess reference.
     *
     * @private {SwitchAccessInterface}
     */
    this.switchAccess_ = switchAccess;

    /**
     * A map from command name to the default key code and function binding for
     * the command.
     *
     * @private {!Object<string, {keyCode: string, callback:function(): void}>}
     */
    this.commandMap_ = this.buildCommandMap_();
  }

  /**
   * Return a list of the names of all user commands.
   *
   * @return {!Array<string>}
   */
  getCommands() {
    return Object.keys(this.commandMap_);
  }

  /**
   * Return the default key code for a command.
   *
   * @param {string} command
   * @return {number}
   */
  getDefaultKeyCodeFor(command) {
    return this.commandMap_[command]['defaultKeyCode'];
  }

  /**
   * Run the function binding for the specified command.
   *
   * @param {string} command
   */
  runCommand(command) {
    this.commandMap_[command]['binding']();
  }

  /**
   * Build the object that maps from command name to the default key code and
   * function binding for the command.
   *
   * @return {!Object<string, {keyCode: string, callback: function(): void}>}
   */
  buildCommandMap_() {
    return {
      'next': {
        'defaultKeyCode': 51, /* '3' key */
        'binding': this.switchAccess_.moveForward.bind(this.switchAccess_)
      },
      'previous': {
        'defaultKeyCode': 50, /* '2' key */
        'binding': this.switchAccess_.moveBackward.bind(this.switchAccess_)
      },
      'select': {
        'defaultKeyCode': 49, /* '1' key */
        'binding': this.switchAccess_.selectCurrentNode.bind(this.switchAccess_)
      },
      'menu': {
        'defaultKeyCode': 52, /* '4' key */
        'binding': this.switchAccess_.enterContextMenu.bind(this.switchAccess_)
      }
    };
  }
}

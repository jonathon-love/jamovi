
'use strict';

class ClipboardPrompt extends HTMLElement {

    constructor() {
        super();

        this._root = this.attachShadow({ mode: 'open' });
        this._host = this._root.host;

        let modifier = (window.navigator.platform === 'MacIntel' ? '&#8984;' : 'Ctrl');

        this._root.innerHTML = `
            <style>
                ${ this._css() }
            </style>
            <div class="content">
                <div class="heading"><div class="icon"></div><div class="title">Copy</div></div>
                <p class="message">The content has been prepared, and can be copied with the button below, or by pressing <em>${ modifier }+C</em> on your keyboard</p>
                <div class="button-box"><button class="copy">Copy</button></div>
                <p class="link"><a href="#" target="_blank">Why is this additional step necessary?</a></p>
                <div contenteditable="true" class="text"></div>
            </div>`;

        this._body = this._root.querySelector('div');
        this._copy = this._body.querySelector('.copy');
        this._textarea = this._body.querySelector('.text');

        this._copy.addEventListener('click', (event) => this._copyClicked());
        this._host.addEventListener('keydown', (event) => this._onKeyDown(event));
    }

    _css() {
        return `
            .text {
                height: 0 ;
                overflow: hidden ;
            }

            .content {

            }

            .heading {
                display: flex;
                align-items: center;
            }

            .content .heading .title {
                font-size: 130%;
            }

            .content .heading .icon {
                height: 30px;
                width: 30px;
                background-image: url('../assets/menu-data-copy.svg');
                background-size: 100% 100%;
                background-position: center;
                background-repeat: no-repeat;
                margin-right: 10px;
            }

            .copy {
                width: 80px;
                line-height: 25px;
                background-color: #3E6DA9;
                color: white;
                border: 1px solid transparent;
                border-radius: 2px;
            }

            .copy:hover {
                background-color: #224a80;
            }

            .message {
                line-height: 1.4;
            }

            .button-box {
                display: flex;
                justify-content: center;
                margin: 20px;
            }

            .link {
                margin-bottom: 6px;
            }
        `;
    }

    copy(content) {
        this._textarea.innerHTML = content;
        this._selectContent();
        return new Promise((resolve, reject) => {
            this._resolve = resolve;
        });
    }

    _selectContent() {
        var selection = window.getSelection();
        var range = document.createRange();
        range.selectNodeContents(this._textarea);
        selection.removeAllRanges();
        selection.addRange(range);
    }

    _onKeyDown(event) {
        let keypressed = event.keyCode || event.which;
        let ctrl = event.ctrlKey || event.metaKey;
        if (keypressed === 13 || (ctrl && event.key === 'c')) { // enter key
            this._copyClicked();
            event.preventDefault();
            event.stopPropagation();
        }
    }

    _copyClicked() {
        this._selectContent();
        let success = document.execCommand('copy');
        this._resolve();
    }

}

customElements.define('jmv-clipboardprompt', ClipboardPrompt);

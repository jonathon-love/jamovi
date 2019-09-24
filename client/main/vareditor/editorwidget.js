
'use strict';

const _ = require('underscore');
const $ = require('jquery');
const Backbone = require('backbone');
Backbone.$ = $;

const keyboardJS = require('keyboardjs');

const NewVarWidget = require('./newvarwidget');
const DataVarWidget = require('./datavarwidget');
const ComputedVarWidget = require('./computedvarwidget');
const RecodedVarWidget = require('./recodedvarwidget');
const FilterWidget = require('./filterwidget');
const VariableListItem = require('./variablelistitem');

const EditorWidget = Backbone.View.extend({
    className: 'EditorWidget',
    initialize(args) {

        this.attached = true;

        this.$el.empty();
        this.$el.addClass('jmv-variable-editor-widget');

        this.$labelBox = $('<div class="label-box"></div>').appendTo(this.$el);
        this.$label = $('<div class="jmv-variable-editor-widget-label"></div>').appendTo(this.$labelBox);
        $('<div class="label-spacer"></div>').appendTo(this.$labelBox);
        this.$importedAs = $('<div class="imported-as single-variable-support"></div>').appendTo(this.$labelBox);
        this.$importedAsLabel = $('<div class="label ">Imported as:</div>').appendTo(this.$importedAs);
        this.$importedAsName = $('<div class="name"></div>').appendTo(this.$importedAs);

        this.model.on('change:importName change:name', event => {
            let columnType = this.model.get('columnType');
            let name = this.model.get('name');
            this._updateImportAsLabel(columnType, name);
        });

        this.$descBox = $('<div class="desc-box"></div>').appendTo(this.$el);

        this.$title = $('<input class="jmv-variable-editor-widget-title single-variable-support" type="text" maxlength="63">').appendTo(this.$descBox);
        this._addTextEvents(this.$title, 'name');
        this.model.on('change:name', event => {
            if ( ! this.attached)
                return;

            let name = this.model.get('name');
            if (name !== this.$title.val())
                this.$title.val(name);
        });
        this.$title.on('blur', () => {
            this.model.set('name', this.$title.val());
        } );

        this.$description = $('<div class="jmv-variable-editor-widget-description single-variable-support" type="text" placeholder="Description" contenteditable="true">').appendTo(this.$descBox);
        this._addTextEvents(this.$description, 'description');
        this.model.on('change:description', event => {
            if ( ! this.attached)
                return;

            let desc = this.model.get('description');
            if (desc !== this.$description[0].textContent)
                this.$description[0].textContent = desc;
        });
        this.$description.on('blur', () => {
            this.model.set('description', this.$description[0].textContent);
        } );

        this.$multiVarBox = $('<div class="multi-var-info"></div>').appendTo(this.$descBox);
        this.$multiVarLabel = $('<div class="multi-var-info-label">Selected:</div>').appendTo(this.$multiVarBox);
        this.$scrollWrapper = $('<div class="scroll-wrapper"></div>').appendTo(this.$multiVarBox);
        this.$multiVarList = $('<div class="multi-var-info-list"></div>').appendTo(this.$scrollWrapper);

        this.model.on('columnChanging', () => {
            if (this.$description.is(":focus"))
                this.$description.blur();
            if (this.$title.is(":focus"))
                this.$title.blur();
        });

        this.$body = $('<div class="jmv-variable-editor-widget-body"></div>').appendTo(this.$el);

        this.$footer = $('<div class="jmv-variable-editor-widget-footer"></div>').appendTo(this.$el);


        let $statusBox = $('<div class="status-box"></div>').appendTo(this.$footer);
        this.$active = $('<div class="active"><div class="switch"></div></div>').appendTo($statusBox);
        let $status = $('<div class="status">Retain unused levels</div>').appendTo($statusBox);

        if (this.model.get('trimLevels') === false)
            this.$active.addClass('retain-levels');
        else
            this.$active.removeClass('retain-levels');

        let activeChanged = (event) => {

            let value = this.$active.hasClass('retain-levels');

            this.model.set('trimLevels', value);
            event.stopPropagation();
            event.preventDefault();
        };

        this.$active.on('click', activeChanged);
        $status.on('click', activeChanged);

        this.model.on('change:trimLevels', event => {
            if (this.model.get('trimLevels') === false)
                this.$active.addClass('retain-levels');
            else
                this.$active.removeClass('retain-levels');
        });

        this.model.on('change:ids', event => this._updateMultiVariableState());
        this.model.dataset.on('columnsChanged', event => this._updateMultiVariableState());

        this.$dataVarWidget = $('<div></div>').appendTo(this.$body);
        this.dataVarWidget = new DataVarWidget({ el: this.$dataVarWidget, model: this.model });
        this.dataVarWidget.setParent(this);

        this.$computedVarWidget = $('<div></div>').appendTo(this.$body);
        this.computedVarWidget = new ComputedVarWidget({ el: this.$computedVarWidget, model: this.model });

        this.$recodedVarWidget = $('<div></div>').appendTo(this.$body);
        this.recodedVarWidget = new RecodedVarWidget({ el: this.$recodedVarWidget, model: this.model });
        this.recodedVarWidget.on('notification', this._notifyEditProblem, this);

        this.$filterWidget = $('<div></div>').appendTo(this.$body);
        this.filterWidget = new FilterWidget({ el: this.$filterWidget, model: this.model.dataset });
        this.filterWidget.on('notification', this._notifyEditProblem, this);

        this.$newVarWidget = $('<div></div>').appendTo(this.$body);
        this.newVarWidget = new NewVarWidget({ el: this.$newVarWidget, model: this.model });

        this.$$widgets = [
            this.$dataVarWidget,
            this.$computedVarWidget,
            this.$recodedVarWidget,
            this.$filterWidget,
            this.$newVarWidget,
        ];
    },
    _updateMultiVariableState() {
        let ids = this.model.get('ids');
        if (ids !== null && this.model.columns.length > 1) {
            this.$el.addClass('multiple-variables');
            this.$multiVarList.empty();
            for (let column of this.model.columns) {
                let item = new VariableListItem(column);
                item.$el.appendTo(this.$multiVarList);
            }
        }
        else
            this.$el.removeClass('multiple-variables');

        this._updateHeading();
    },
    _updateImportAsLabel(columnType, name) {
        if (columnType === 'data' || columnType === 'computed' || columnType === 'recoded') {
            let importName = this.model.get('importName');
            if (importName !== name && importName !== '') {
                if (importName !== this.$importedAsName[0].textContent)
                    this.$importedAsName[0].textContent = importName;
                this.$importedAs.show();
            }
            else
                this.$importedAs.hide();
        }
    },
    _addTextEvents($element, propertyName) {
        $element.focus(() => {
            keyboardJS.pause('');
            $element.select();
        } );

        $element.blur(() => {
            keyboardJS.resume();
        } );

        $element.keydown((event) => {
            var keypressed = event.keyCode || event.which;
            if (keypressed === 13) { // enter key
                $element.blur();
                event.preventDefault();
                event.stopPropagation();
            }
            else if (keypressed === 27) { // escape key
                $element.blur();
                if (this.model.get('changes'))
                    this.model.revert();
                event.preventDefault();
                event.stopPropagation();
            }
        });
    },
    _show($widget) {
        let $$widgets = this.$$widgets;
        for (let i = 0; i < $$widgets.length; i++) {
            if ( ! $widget[0].isSameNode($$widgets[i][0]))
                $$widgets[i].hide();
        }
        $widget.show();
    },
    detach() {
        this.attached = false;

        this.dataVarWidget.detach();
        this.computedVarWidget.detach();
        this.newVarWidget.detach();
        this.recodedVarWidget.detach();
        this.filterWidget.detach();
    },
    _updateHeading() {
        let type = this.model.get('columnType');
        let ids = this.model.get('ids');
        let multiSupport = ids !== null && this.model.columns.length > 1;
        if (type === 'data') {
            if (multiSupport)
                this.$label[0].textContent = 'MULTIPLE DATA VARIABLES (' + this.model.columns.length + ')';
            else
                this.$label[0].textContent = 'DATA VARIABLE';
        }
        else if (type === 'computed') {
            if (multiSupport)
                this.$label[0].textContent = 'MULTIPLE COMPUTED VARIABLES (' + this.model.columns.length + ')';
            else
                this.$label[0].textContent = 'COMPUTED VARIABLE';
        }
        else if (type === 'recoded') {
            if (multiSupport)
                this.$label[0].textContent = 'MULTIPLE TRANSFORMED VARIABLES (' + this.model.columns.length + ')';
            else
                this.$label[0].textContent = 'TRANSFORMED VARIABLE';
        }
        else if (type === 'filter') {
            this.$label[0].textContent = 'ROW FILTERS';
        }
    },
    attach() {
        this.attached = true;
        let name = this.model.get('name');
        if (name !== this.$title.val())
            this.$title.val(name);

        let description = this.model.get('description');
        if (description !== this.$description[0].textContent)
            this.$description[0].textContent = description;

        if (this.model.get('trimLevels') === false)
            this.$active.addClass('retain-levels');
        else
            this.$active.removeClass('retain-levels');

        let type = this.model.get('columnType');

        this._updateImportAsLabel(type, name);

        this._updateHeading();

        if (type === 'data') {
            this.$footer.show();
            this.$labelBox.show();
            this.$label.show();
            this.$title.show();
            this.$description.show();
            this._show(this.$dataVarWidget);
            this.dataVarWidget.attach();
        }
        else if (type === 'computed') {
            this.$footer.show();
            this.$labelBox.show();
            this.$label.show();
            this.$title.show();
            this.$description.show();
            this._show(this.$computedVarWidget);
            this.computedVarWidget.attach();
        }
        else if (type === 'recoded') {
            this.$footer.show();
            this.$labelBox.show();
            this.$label.show();
            this.$title.show();
            this.$description.show();
            this._show(this.$recodedVarWidget);
            this.recodedVarWidget.attach();
        }
        else if (type === 'filter') {
            this.$footer.hide();
            this.$labelBox.show();
            this.$importedAs.hide();
            this.$label.show();
            this.$title.hide();
            this.$description.hide();
            this._show(this.$filterWidget);
            this.filterWidget.attach();
        }
        else {
            this.$footer.hide();
            this.$labelBox.hide();
            this.$title.hide();
            this.$description.hide();
            this._show(this.$newVarWidget);
            this.newVarWidget.attach();
        }
    },
    update() {
        let type = this.model.get('columnType');
        if (type === 'filter')
            this.filterWidget.update();
    },
    _notifyEditProblem(note) {
        this.trigger('notification', note);
    }
});

module.exports = EditorWidget;

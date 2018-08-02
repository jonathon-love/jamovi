
import ast
import re

from jamovi.core import ColumnType
from jamovi.core import MeasureType
from jamovi.core import DataType

from .compute import Parser
from .compute import FormulaStatus
from .compute import Transmogrifier
from .compute import Transfilterifier
from .compute import Transfudgifier
from .compute import Checker

from .utils import FValues
from .utils import convert
from .utils import is_missing


NaN = float('nan')


class Column:

    def __init__(self, parent, child=None):
        self._parent = parent
        self._child = child
        self._id = -1
        self._index = -1
        self._description = ''
        self._hidden = False
        self._filter_no = -1
        self._trim_levels = True

        self._node = None
        self._fields = ('name',)  # for AST compatibility
        self._node_parents = [ ]
        self._needs_recalc = False
        self._formula_status = FormulaStatus.EMPTY

    def _create_child(self):
        if self._child is None:
            self._parent._realise_column(self)

    def __setitem__(self, index, value):
        if self._child is None:
            self._create_child()
        self._child.set_value(index, value)

    def set_value(self, index, value):
        if self._child is None:
            self._create_child()
        self._child.set_value(index, value)

    def __getitem__(self, index):
        if self._child is not None:
            return self._child[index]
        else:
            return (-2147483648, '')

    def fvalue(self, index, filt):
        if self._child is not None:
            if filt and self._parent.is_row_filtered(index):
                return (-2147483648, '')
            v = self._child[index]
            if self._child.data_type is DataType.INTEGER and self.has_levels:
                if is_missing(v):
                    return (-2147483648, '')
                else:
                    return (v, self._child.get_label(v))
            else:
                return v
        else:
            return (-2147483648, '')

    def fvalues(self, filt):
        return FValues(self, filt)

    def is_atomic_node(self):
        return False

    def prep_for_deletion(self):
        # removes itself as a dependent
        if self._node is not None:
            self._node._remove_node_parent(self)
            self._node = None

    @property
    def is_filter(self):
        return self.column_type is ColumnType.FILTER

    @property
    def is_virtual(self):
        return self._child is None

    def realise(self):
        self._create_child()

    @property
    def id(self):
        if self._child is not None:
            return self._child.id
        return self._id

    @id.setter
    def id(self, id):
        self._id = id
        if self._child is not None:
            self._child.id = id

    @property
    def description(self):
        return self._description

    @description.setter
    def description(self, description):
        self._description = description

    @property
    def hidden(self):
        return self._hidden

    @hidden.setter
    def hidden(self, hidden):
        self._hidden = hidden

    @property
    def active(self):
        if self._child is not None:
            return self._child.active
        return True

    @active.setter
    def active(self, active):
        if self._child is None:
            self._create_child()
        self._child.active = active

    @property
    def filter_no(self):
        return self._filter_no

    @filter_no.setter
    def filter_no(self, filter_no):
        self._filter_no = filter_no

    @property
    def trim_levels(self):
        if self._child is not None:
            return self._child.trim_levels
        return True

    @trim_levels.setter
    def trim_levels(self, trim_levels):
        if self._child is None:
            self._create_child()
        self._child.trim_levels = trim_levels

    @property
    def index(self):
        return self._index

    @index.setter
    def index(self, index):
        self._index = index

    @property
    def name(self):
        if self._child is not None:
            return self._child.name
        return ''

    @name.setter
    def name(self, name):
        if self._child is None:
            self._create_child()
        self._child.name = name

    @property
    def import_name(self):
        if self._child is not None:
            return self._child.import_name
        return ''

    @property
    def column_type(self):
        if self._child is not None:
            return self._child.column_type
        return ColumnType.NONE

    @column_type.setter
    def column_type(self, column_type):
        if self._child is None:
            self._create_child()
        self._child.column_type = column_type

    @property
    def data_type(self):
        if self._child is not None:
            return self._child.data_type
        return DataType.INTEGER

    @property
    def measure_type(self):
        if self._child is not None:
            return self._child.measure_type
        return MeasureType.NONE

    @measure_type.setter
    def measure_type(self, measure_type):
        if self._child is None:
            self._create_child()
        self._child.measure_type = measure_type

    @property
    def auto_measure(self):
        if self._child is not None:
            return self._child.auto_measure
        return True

    @auto_measure.setter
    def auto_measure(self, auto):
        if self._child is None:
            self._create_child()
        self._child.auto_measure = auto

    @property
    def dps(self):
        if self._child is not None:
            return self._child.dps
        return 0

    @dps.setter
    def dps(self, dps):
        if self._child is None:
            self._create_child()
        self._child.dps = dps

    @property
    def formula(self):
        if self._child is not None:
            return self._child.formula
        return ''

    @formula.setter
    def formula(self, formula):
        if self._child is None:
            self._create_child()
        regex = re.compile(r'\s+')
        formula = formula.strip()
        formula = regex.sub(' ', formula)
        if formula != self._child.formula:
            self._child.formula = formula
            self.parse_formula()

    @property
    def formula_message(self):
        if self._child is not None:
            return self._child.formula_message
        return ''

    @formula_message.setter
    def formula_message(self, formula_message):
        if self._child is None:
            self._create_child()
        self._child.formula_message = formula_message

    @property
    def has_formula(self):
        return self.formula != ''

    def determine_dps(self):
        if self._child is not None:
            self._child.determine_dps()

    def append(self, value):
        if self._child is None:
            self._create_child()
        self._child.append(value)

    def insert_level(self, raw, label, importValue=None):
        if self._child is None:
            self._create_child()
        self._child.insert_level(raw, label, importValue)

    def get_label(self, value):
        if self._child is None:
            raise RuntimeError('Virtual columns have no labels')
        return self._child.get_label(value)

    def get_value_for_label(self, label):
        if self._child is not None:
            return self._child.get_value_for_label(label)
        else:
            return -2147483648

    def clear_levels(self):
        if self._child is None:
            self._create_child()
        self._child.clear_levels()

    @property
    def has_levels(self):
        if self._child is not None:
            return self._child.has_levels
        return False

    @property
    def level_count(self):
        if self._child is not None:
            return self._child.level_count
        return 0

    def has_level(self, index_or_name):
        if self._child is not None:
            return self._child.has_level(index_or_name)
        return False

    @property
    def levels(self):
        if self._child is not None:
            return self._child.levels
        return []

    def append_level(self, raw, label, import_value=None):
        if self._child is not None:
            return self._child.append_level(raw, label, import_value)
        return False

    @property
    def row_count(self):
        if self._child is not None:
            return self._child.row_count
        return 0

    @property
    def changes(self):
        if self._child is not None:
            return self._child.changes
        return False

    def clear_at(self, index):
        if self._child is None:
            self._create_child()
        self._child.clear_at(index)

    def __iter__(self):
        if self._child is None:
            self._create_child()
        return self._child.__iter__()

    def raw(self, index):
        if self._child is not None:
            return self._child.raw(index)
        return -2147483648

    def set_data_type(self, data_type):
        if self._child is None:
            self._create_child()
        self._child.set_data_type(data_type)

    def set_measure_type(self, measure_type):
        if self._child is None:
            self._create_child()
        self._child.set_measure_type(measure_type)

    def change(self,
               data_type=DataType.NONE,
               measure_type=MeasureType.NONE,
               levels=None):

        if self._child is None:
            self._create_child()

        self._child.change(
            data_type=data_type,
            measure_type=measure_type,
            levels=levels)

    @property
    def has_deps(self):
        return len(self.dependencies) != 0 or len(self.dependents) != 0

    @property
    def dependencies(self):
        rer = Column.DependencyResolver()
        rer.visit(self)
        return rer.columns

    @property
    def dependents(self):
        rer = Column.DependentResolver()
        rer.visit(self)
        return rer.columns

    @property
    def needs_recalc(self):
        if self.column_type is not ColumnType.COMPUTED and self.column_type is not ColumnType.FILTER:
            return False
        else:
            return self._needs_recalc

    @needs_recalc.setter
    def needs_recalc(self, needs_recalc: bool):
        for parent in self._node_parents:
            parent.needs_recalc = needs_recalc
        if self.column_type is ColumnType.COMPUTED or self.column_type is ColumnType.FILTER:
            self._needs_recalc = needs_recalc

    def recalc(self, start=None, end=None):

        if not self.needs_recalc:
            return

        for dep in self.dependencies:
            if dep.needs_recalc:
                dep.recalc()

        if start is None:
            start = 0
            end = self.row_count
        elif end is None:
            end = start + 1

        self._child.clear_levels()

        if self._node is not None and self._node.has_levels:
            for level in self._node.levels:
                self._child.append_level(level[0], level[1])

        if self.data_type is DataType.DECIMAL:
            ul_type = float
        elif self.data_type is DataType.TEXT:
            ul_type = str
        else:
            ul_type = int

        if self._node is None:
            if not self.is_filter:
                v = convert(NaN, ul_type)
            else:
                v = 1
            for row_no in range(start, end):
                self._child.set_value(row_no, v, True)
        else:
            for row_no in range(start, end):
                try:
                    if self.is_filter:
                        v = self._node.fvalue(row_no, False)
                    elif self.uses_column_formula and self._parent.is_row_filtered(row_no):
                        v = NaN
                    else:
                        v = self._node.fvalue(row_no, self.uses_column_formula)
                    v = convert(v, ul_type)
                except Exception as e:
                    if not self.is_filter:
                        v = convert(NaN, ul_type)
                    else:
                        v = 1
                    self._parent._log.exception(e)
                self._child.set_value(row_no, v, True)
            self.determine_dps()

        self._needs_recalc = False

    def parse_formula(self):
        try:
            dataset = self._parent

            if self._formula_status is FormulaStatus.OK:
                self._node._remove_node_parent(self)
                self._node = None

            node = Parser.parse(self.formula)
            self._child.formula_message = ''

            if node is not None:
                node = Transfudgifier().visit(node)

            if self.column_type is ColumnType.FILTER:
                if node is None:
                    node = ast.Num(1)
                else:
                    node = ast.Call(
                        func=ast.Name(id='IFMISS', ctx=ast.Load()),
                        args=[
                            node,
                            ast.Num(0),
                            node ],
                        keywords=[ ] )

                parent_filter_no = self.filter_no - 1
                parent_filter_start = None

                while parent_filter_no >= 0:
                    for i in range(self.index):
                        parent = self._parent[i]
                        if parent.filter_no == parent_filter_no:
                            if parent.active:
                                if parent_filter_start is None:
                                    parent_filter_start = i
                                    break
                            else:
                                parent_filter_no -= 1
                    if parent_filter_start is not None:
                        break

                if parent_filter_no >= 0:
                    parent_filter_end = self.index
                    for i in range(parent_filter_start, self.index):
                        filter_no = self._parent[i].filter_no
                        if filter_no < parent_filter_no:
                            pass
                        elif filter_no == parent_filter_no:
                            if parent_filter_start is None:
                                parent_filter_start = i
                            parent_filter_end = i + 1
                        else:
                            break

                    parents = list(map(
                        lambda i: ast.Name(id=self._parent[i].name, ctx=ast.Load()),
                        range(parent_filter_start, parent_filter_end)))
                    ops = list(map(
                        lambda i: ast.Eq(),
                        range(parent_filter_start, parent_filter_end)))

                    Transfilterifier(parents).visit(node)

                    node = ast.Call(
                        func=ast.Name(id='IF', ctx=ast.Load()),
                        args=[
                            ast.Compare(
                                left=ast.Num(1),
                                ops=ops,
                                comparators=parents),
                            node,
                            ast.Num(-2147483648) ],
                        keywords=[ ] )

            if node is None:
                self._formula_status = FormulaStatus.EMPTY
            else:
                Checker.check(self, node)
                node = Transmogrifier(dataset).visit(node)

                self._node = node
                self._node._add_node_parent(self)
                self._formula_status = FormulaStatus.OK

                self.set_data_type(self._node.data_type)
                self.set_measure_type(self._node.measure_type)

        except RecursionError:
            self._formula_status = FormulaStatus.ERROR
            self._child.formula_message = 'Circular reference detected'
        except SyntaxError:
            self._formula_status = FormulaStatus.ERROR
            self._child.formula_message = 'The formula is mis-specified'
        except (NameError, TypeError, ValueError) as e:
            self._formula_status = FormulaStatus.ERROR
            self._child.formula_message = str(e)
        except BaseException as e:
            self._formula_status = FormulaStatus.ERROR
            self._child.formula_message = 'Unexpected error ({}, {})'.format(str(e), type(e).__name__)
            # import traceback
            # print(traceback.format_exc())

    def _add_node_parent(self, parent):
        self._node_parents.append(parent)

    def _remove_node_parent(self, parent):
        self._node_parents.remove(parent)

    @property
    def uses_column_formula(self):
        if self._node is not None:
            return self._node.uses_column_formula
        else:
            return False

    class DependentResolver:

        def __init__(self):
            self._columns = set()

        def visit(self, node):
            for parent in node._node_parents:
                if isinstance(parent, Column):
                    self._columns.add(parent)
                    for grand_parent in parent._node_parents:
                        self.visit(grand_parent)
                else:
                    self.visit(parent)

        @property
        def columns(self):
            return self._columns

    class DependencyResolver(ast.NodeVisitor):

        def __init__(self):
            self._first = True
            self._columns = set()

        def visit(self, node):
            if self._first and isinstance(node, Column):
                self._first = False
                if node._node is None:
                    return
                else:
                    return self.visit(node._node)
            else:
                return ast.NodeVisitor.visit(self, node)

        def visit_Column(self, column):
            self._columns.add(column)
            if column._node is not None:
                self.visit(column._node)

        def visit_BinOp(self, node):
            self.visit(node.left)
            self.visit(node.right)

        def visit_UnaryOp(self, node):
            self.visit(node.operand)

        def visit_Call(self, node):
            for arg in node.args:
                self.visit(arg)

        @property
        def columns(self):
            return self._columns

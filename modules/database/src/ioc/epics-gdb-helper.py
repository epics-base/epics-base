#*************************************************************************
# SPDX-License-Identifier: EPICS
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

# see https://sourceware.org/gdb/current/onlinedocs/gdb.html/Python.html
# also documentation/gdb.md in epics-base
#
# Troubleshooting:
#  (gdb) set python print-stack full

import time

import gdb
import gdb.printing

voidp = gdb.lookup_type('void').pointer()

def ellIter(plist : gdb.Value, cast :gdb.Type =None): # -> ELLNODE*
    # Iterate an ELLLIST*
    if plist.type.code == gdb.TYPE_CODE_PTR:
        plist = plist.dereference()
    pnode = plist['node']['next']

    while pnode:
        yield pnode if cast is None else pnode.cast(cast)
        pnode = pnode.dereference()['next']

class Wrapper(object):
    def __init__(self, val: gdb.Value):
        self._v = val
    def to_string(self) -> str:
        raise NotImplementedError()

class epicsTimeStamp(Wrapper):
    def to_string(self):
        sec = int(self._v['secPastEpoch'])
        ns = int(self._v['nsec'])
        T = sec + 631152000 + 1e-9*ns
        return f'epicsTimeStamp(secPastEpoch = {sec}, ns = {ns}, {time.ctime(T)!r})'

class dbCommon(Wrapper):
    def __init__(self, val: gdb.Value, name='dbCommon'):
        self._v, self._name = val, name
    def to_string(self):
        try:
            return '(%s*)(%s, "%s")'%(self._name, self._v.address.cast(voidp), self._v['name'].string())
        except Exception:
            raise ValueError(self._v.type)

class dbAddr(Wrapper):
    def to_string(self):
        v = self._v # Value
        rec = v['precord'].dereference()['name'].string()
        fld = v['pfldDes'].dereference()['name'].string()
        return '(dbAddr*)(%s, "%s.%s")'%(self._v.address.cast(voidp), rec, fld)

class dbChannel(Wrapper):
    def to_string(self):
        v = self._v # Value
        rec = v['addr']['precord'].dereference()['name'].string()
        fld = v['addr']['pfldDes'].dereference()['name'].string()
        return '(dbChannel*)(%s, "%s.%s")'%(self._v.address.cast(voidp), rec, fld)

class dbLink(Wrapper):
    def to_string(self):
        v = self._v # Value

        # size_t offset = (char*)&v - (char*)v.precord
        offset = int(v.address) - int(v['precord'])
        # dbRecordType& rdes = *v.precord->rdes
        rdes = v['precord'].dereference()['rdes'].dereference()

        no_links = int(rdes['no_links'])
        link_ind = rdes['link_ind'] # array of indicies of link fields
        papFldDes = rdes['papFldDes'] # main array of field descriptions

        for idx in range(no_links):
            idx = link_ind[idx]
            fdes = papFldDes[idx].dereference() # dbFldDes
            if int(fdes['offset']) == offset:
                fld = fdes['name'].string()
                break
        else:
            fld = '???'

        rec = v['precord'].dereference()['name'].string()
        ltype = int(v['type'])
        if ltype in (10, 11):
            target = ', target="%s"'%v['value']['pv_link']['pvname'].string()
        elif ltype==12:
            target = ', target="@%s"'%v['value']['instio']['string'].string()
        else:
            ltype = {
                0: 'CONSTANT',
                1: 'PV_LINK',
                10: 'DB_LINK',
                11: 'CA_LINK',
                12: 'INST_IO',
            }.get(ltype, 'LINK(%s)'%ltype)
            target = ', type=%s'%ltype

        return '(link*)(%s, src="%s.%s"%s)'%(self._v.cast(voidp), rec, fld, target)

class EPICSPrinter(gdb.printing.PrettyPrinter):
    _pointers = {
        'epicsTimeStamp': epicsTimeStamp,
        'dbCommon': dbCommon,
        'dbChannel': dbChannel,
        'dbAddr': dbAddr,
        'DBADDR': dbAddr,
        'link': dbLink,
    }
    _values = {
        'epicsTimeStamp': epicsTimeStamp,
        'link': dbLink,
    }
    def __init__(self):
        gdb.printing.PrettyPrinter.__init__(self, 'EPICS')
    @classmethod
    def __call__(klass, val: gdb.Value) -> Wrapper:
        isptr = val.type.code == gdb.TYPE_CODE_PTR
        if isptr:
            if val.type==voidp or not val: # NULL
                return
            val = val.dereference()
            tbl = klass._pointers
        else:
            tbl = klass._values

        tname = val.type.unqualified().strip_typedefs().name # str
        printer = tbl.get(tname)

        if isptr and printer is None and tname is not None and tname.endswith('Record'):
            # exclude eg. lockRecord and processNotifyRecord which are not record types
            # which are all too small to be records
            if val.type.sizeof >= gdb.lookup_type('dbCommon').sizeof:
                return dbCommon(val, str(tname))
        if printer is not None:
            return printer(val)

gdb.printing.register_pretty_printer(None, EPICSPrinter(), replace=True)

class RCast(gdb.Function):
    #Cast dbCommon* to specific record type
    def __init__(self):
        super(RCast, self).__init__("rcast")
    @staticmethod
    def invoke(prec: gdb.Value) -> gdb.Value:
        if prec.type.code!=gdb.TYPE_CODE_PTR:
            return prec

        ptype = prec.type.target().unqualified().strip_typedefs()
        if ptype.name=='dbCommon':
            rname = prec.dereference()['rdes'].dereference()['name'].string() # eg. 'calc'
            rtype = gdb.lookup_type(f'{rname}Record')
            return prec.cast(rtype.pointer())

        return prec
RCast()

class Rec(gdb.Function):
    #Lookup record by name
    def __init__(self):
        super(Rec, self).__init__("record")
    @staticmethod
    def invoke(prec: gdb.Value) -> gdb.Value:
        target = prec.string()
        pdbbase = gdb.lookup_global_symbol('pdbbase').value() # dbBase*
        if not pdbbase:
            raise RuntimeError('Process database not yet initialized.  Load some records first.')
        pdbbase = pdbbase.dereference()
        dbRecordType = gdb.lookup_type('dbRecordType').pointer()
        dbRecordNode = gdb.lookup_type('dbRecordNode').pointer()
        dbCommon = gdb.lookup_type('dbCommon').pointer()

        for rtype in ellIter(pdbbase['recordTypeList'].address, dbRecordType):
            for rnode in ellIter(rtype.dereference()['recList'].address, dbRecordNode):
                precname = rnode.dereference()['recordname'].string()
                if precname==target:
                    return RCast.invoke(rnode.dereference()['precord'].cast(dbCommon))

        raise RuntimeError(f'No record {target!r}')
Rec()

class EllNth(gdb.Function):
    # Fetch entry in list
    def __init__(self):
        super(EllNth, self).__init__("elln")
    @staticmethod
    def invoke(plist: gdb.Value, idx: gdb.Value) -> gdb.Value:
        idx = int(idx)
        if idx < 0:
            raise ValueError('Negative index')

        it = ellIter(plist)
        for _n in range(idx):
            next(it) # discard
        return next(it)
EllNth()

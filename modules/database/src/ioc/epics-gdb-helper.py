try:
    import gdb
except ImportError:
    gdb = None

class Wrapper(object):
    def __init__(self, val):
        self._v = val

class dbCommon(object):
    def __init__(self, val, name='dbCommon'):
        self._v, self._name = val, name
    def to_string(self):
        return '(%s*)(%s, "%s")'%(self._name, self._v.cast(voidp), self._v.dereference()['name'].string())

class dbAddr(Wrapper):
    def to_string(self):
        v = self._v.dereference()
        rec = v['precord'].dereference()['name'].string()
        fld = v['pfldDes'].dereference()['name'].string()
        return '(dbAddr*)(%s, "%s.%s")'%(self._v.cast(voidp), rec, fld)

class dbChannel(Wrapper):
    def to_string(self):
        v = self._v.dereference()
        rec = v['addr']['precord'].dereference()['name'].string()
        fld = v['addr']['pfldDes'].dereference()['name'].string()
        return '(dbChannel*)(%s, "%s.%s")'%(self._v.cast(voidp), rec, fld)

class dbLink(Wrapper):
    def to_string(self):
        v = self._v.dereference()

        offset = int(self._v.cast(charp) - v['precord'].cast(charp))
        rdes = v['precord'].dereference()['rdes'].dereference()

        no_links = int(rdes['no_links'])
        link_ind = rdes['link_ind']
        papFldDes = rdes['papFldDes']

        for idx in range(no_links):
            idx = link_ind[idx]
            fdes = papFldDes[idx].dereference()
            if int(fdes['offset']) == offset:
                fld = fdes['name'].string()
                break
        else:
            fld = '???'

        rec = v['precord'].dereference()['name'].string()
        ltype = int(v['type'])
        if ltype in (10, 11):
            target = ', target="%s"'%v['value']['pv_link']['pvname'].string()
        elif ltype==(12):
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
    _printers = {
        'dbCommon': dbCommon,
        'dbChannel': dbChannel,
        'dbAddr': dbAddr,
        'DBADDR': dbAddr,
        'link': dbLink,
        'DBLINK': dbLink,
    }
    def __init__(self):
        gdb.printing.PrettyPrinter.__init__(self, 'EPICS')
    def __call__(self, val):
        if val.type.code==gdb.TYPE_CODE_PTR:
            ptype = val.type.target().unqualified()
            printer = self._printers.get(ptype.name)
            if printer is None and ptype.name is not None and ptype.name.endswith('Record'):
                return dbCommon(val, str(ptype.name))
            if printer is not None:
                return printer(val)

if gdb is not None:

    voidp = gdb.lookup_type('void').pointer()
    charp = gdb.lookup_type('char').pointer()

    gdb.printing.register_pretty_printer(None, EPICSPrinter())

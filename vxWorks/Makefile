#*************************************************************************
# Copyright (c) 2017 UChicago Argonne LLC, as Operator of Argonne
#     National Laboratory.
# EPICS BASE is distributed subject to a Software License Agreement found
# in file LICENSE that is included with this distribution.
#*************************************************************************

TOP = ..
include $(TOP)/configure/CONFIG

# Install Boost smart_ptr headers needed by VxWorks 6.x

INC_vxWorks += boost/assert.hpp
INC_vxWorks += boost/checked_delete.hpp
INC_vxWorks += boost/config/auto_link.hpp
INC_vxWorks += boost/config/compiler/gcc.hpp
INC_vxWorks += boost/config.hpp
INC_vxWorks += boost/config/no_tr1/memory.hpp
INC_vxWorks += boost/config/no_tr1/utility.hpp
INC_vxWorks += boost/config/platform/vxworks.hpp
INC_vxWorks += boost/config/posix_features.hpp
INC_vxWorks += boost/config/select_compiler_config.hpp
INC_vxWorks += boost/config/select_platform_config.hpp
INC_vxWorks += boost/config/select_stdlib_config.hpp
INC_vxWorks += boost/config/stdlib/dinkumware.hpp
INC_vxWorks += boost/config/suffix.hpp
INC_vxWorks += boost/config/user.hpp
INC_vxWorks += boost/current_function.hpp
INC_vxWorks += boost/detail/sp_typeinfo.hpp
INC_vxWorks += boost/detail/workaround.hpp
INC_vxWorks += boost/enable_shared_from_this.hpp
INC_vxWorks += boost/exception/detail/attribute_noreturn.hpp
INC_vxWorks += boost/exception/exception.hpp
INC_vxWorks += boost/memory_order.hpp
INC_vxWorks += boost/shared_ptr.hpp
INC_vxWorks += boost/smart_ptr/bad_weak_ptr.hpp
INC_vxWorks += boost/smart_ptr/detail/operator_bool.hpp
INC_vxWorks += boost/smart_ptr/detail/shared_count.hpp
INC_vxWorks += boost/smart_ptr/detail/sp_convertible.hpp
INC_vxWorks += boost/smart_ptr/detail/sp_counted_base_gcc_ppc.hpp
INC_vxWorks += boost/smart_ptr/detail/sp_counted_base_gcc_x86.hpp
INC_vxWorks += boost/smart_ptr/detail/sp_counted_base.hpp
INC_vxWorks += boost/smart_ptr/detail/sp_counted_impl.hpp
INC_vxWorks += boost/smart_ptr/detail/sp_has_sync.hpp
INC_vxWorks += boost/smart_ptr/detail/spinlock.hpp
INC_vxWorks += boost/smart_ptr/detail/spinlock_pool.hpp
INC_vxWorks += boost/smart_ptr/detail/spinlock_pt.hpp
INC_vxWorks += boost/smart_ptr/detail/spinlock_sync.hpp
INC_vxWorks += boost/smart_ptr/detail/sp_nullptr_t.hpp
INC_vxWorks += boost/smart_ptr/detail/yield_k.hpp
INC_vxWorks += boost/smart_ptr/enable_shared_from_this.hpp
INC_vxWorks += boost/smart_ptr/shared_ptr.hpp
INC_vxWorks += boost/smart_ptr/weak_ptr.hpp
INC_vxWorks += boost/throw_exception.hpp
INC_vxWorks += boost/tr1/detail/config.hpp
INC_vxWorks += boost/tr1/memory.hpp
INC_vxWorks += boost/type_traits/is_signed.hpp
INC_vxWorks += boost/weak_ptr.hpp

# Base doesn't have a vpath for .hpp files
vpath %.hpp $(USR_VPATH) $(ALL_SRC_DIRS)

include $(TOP)/configure/RULES

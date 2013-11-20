NULL=
INTERFACES_DIR = $(top_builddir)/src/common/dbus/interfaces
DBUS_BUILT_SOURCES = \
    gum-dbus-user-gen.c \
    gum-dbus-user-gen.h \
    gum-dbus-user-service-gen.c \
    gum-dbus-user-service-gen.h \
    gum-dbus-group-gen.c \
    gum-dbus-group-gen.h \
    gum-dbus-group-service-gen.c \
    gum-dbus-group-service-gen.h \
    $(NULL)

DBUS_BUILT_DOCS = \
    gum-dbus-user-doc-gen-org.tizen.SecurityAccounts.gUserManagement.User.xml \
    gum-dbus-user-doc-gen-org.tizen.SecurityAccounts.gUserManagement.UserService.xml \
    gum-dbus-group-doc-gen-org.tizen.SecurityAccounts.gUserManagement.Group.xml \
    gum-dbus-user-doc-gen-org.tizen.SecurityAccounts.gUserManagement.GroupService.xml \
    $(NULL)

DBUS_INTERFACE_PREFIX="org.tizen.SecurityAccounts.gUserManagement."
gum-dbus-user-gen.c gum-dbus-user-gen.h : $(INTERFACES_DIR)/org.tizen.SecurityAccounts.gUserManagement.User.xml
	gdbus-codegen                                       \
       --interface-prefix $(DBUS_INTERFACE_PREFIX)      \
       --c-namespace GumDbus                       \
       --generate-c-code  gum-dbus-user-gen     \
       --generate-docbook gum-dbus-user-doc-gen \
       $<
       
gum-dbus-user-service-gen.c gum-dbus-user-service-gen.h : $(INTERFACES_DIR)/org.tizen.SecurityAccounts.gUserManagement.UserService.xml
	gdbus-codegen                                       \
       --interface-prefix $(DBUS_INTERFACE_PREFIX)      \
       --c-namespace GumDbus                       \
       --generate-c-code  gum-dbus-user-service-gen     \
       --generate-docbook gum-dbus-user-service-doc-gen \
       $<
       
gum-dbus-group-gen.c gum-dbus-group-gen.h : $(INTERFACES_DIR)/org.tizen.SecurityAccounts.gUserManagement.Group.xml
	gdbus-codegen                                       \
       --interface-prefix $(DBUS_INTERFACE_PREFIX)      \
       --c-namespace GumDbus                       \
       --generate-c-code  gum-dbus-group-gen     \
       --generate-docbook gum-dbus-group-doc-gen \
       $<

gum-dbus-group-service-gen.c gum-dbus-group-service-gen.h : $(INTERFACES_DIR)/org.tizen.SecurityAccounts.gUserManagement.GroupService.xml
	gdbus-codegen                                       \
       --interface-prefix $(DBUS_INTERFACE_PREFIX)      \
       --c-namespace GumDbus                       \
       --generate-c-code  gum-dbus-group-service-gen     \
       --generate-docbook gum-dbus-group-service-doc-gen \
       $<

noinst_LTLIBRARIES = libgum-dbus-glue.la

libgum_dbus_glue_la_CPPFLAGS = \
    $(GUM_COMMON_INCLUDES) \
    $(GUM_COMMON_CFLAGS) \
    $(NULL)

libgum_dbus_glue_la_LIBADD = \
    $(GUM_COMMON_LIBS) \
    $(NULL)

libgum_dbus_glue_la_SOURCES =  \
   $(DBUS_BUILT_SOURCES) \
   $(NULL)

if USE_DBUS_SERVICE
dbusinterfacesdir = $(DBUS_INTERFACES_DIR)
dbusinterfaces_DATA = \
    $(INTERFACES_DIR)/org.tizen.SecurityAccounts.gUserManagement.User.xml \
    $(INTERFACES_DIR)/org.tizen.SecurityAccounts.gUserManagement.UserService.xml \
    $(INTERFACES_DIR)/org.tizen.SecurityAccounts.gUserManagement.Group.xml \
    $(INTERFACES_DIR)/org.tizen.SecurityAccounts.gUserManagement.GroupService.xml
endif

EXTRA_DIST = interfaces

clean-local :
	rm -f *~ $(DBUS_BUILT_SOURCES) $(DBUS_BUILT_DOCS)


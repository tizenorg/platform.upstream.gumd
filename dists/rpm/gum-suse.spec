# define used dbus type [p2p, session, system]
%define dbus_type system
# enable debug features such as control environment variables
# WARNING! do not use for production builds as it will break security
%define debug_build 0


Name: gumd
Summary: User management daemon and client library
Version: 0.0.3
Release: 1
Group: System/Daemons
License: LGPL-2.1+
Source: %{name}-%{version}.tar.gz
URL: https://github.com/01org/gumd
Requires:   libgum = %{version}-%{release}
%if %{dbus_type} != "p2p"
Requires: dbus-1
%endif
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires: pkgconfig(systemd)
BuildRequires: pkgconfig(dbus-1)
BuildRequires: pkgconfig(gtk-doc)
BuildRequires: pkgconfig(glib-2.0) >= 2.30
BuildRequires: pkgconfig(gobject-2.0)
BuildRequires: pkgconfig(gio-2.0)
BuildRequires: pkgconfig(gio-unix-2.0)
BuildRequires: pkgconfig(gmodule-2.0)


%description
%{summary}.


%package -n libgum
Summary:    User management client library
Group:      System/Libraries


%description -n libgum
%{summary}.


%package -n libgum-devel
Summary:    Development files for user management client library
Group:      Development/Libraries
Requires:   libgum = %{version}-%{release}


%description -n libgum-devel
%{summary}.


%package doc
Summary:    Documentation files for %{name}
Group:      Development/Libraries
Requires:   libgum = %{version}-%{release}


%description doc
%{summary}.


%prep
%setup -q -n %{name}-%{version}


%build
%if %{debug_build} == 1
%configure --enable-dbus-type=%{dbus_type} --enable-debug
%else
%configure --enable-dbus-type=%{dbus_type}
%endif


make %{?_smp_mflags}


%install
rm -rf %{buildroot}
%make_install


%post
/sbin/ldconfig
chmod u+s %{_bindir}/%{name}
groupadd -f -r gumd


%postun -p /sbin/ldconfig


%files -n libgum
%defattr(-,root,root,-)
%{_libdir}/libgum*.so.*
%{_bindir}/gum-utils


%files -n libgum-devel
%defattr(-,root,root,-)
%{_includedir}/gum/*
%{_libdir}/libgum*.so
%{_libdir}/libgum*.la
%{_libdir}/pkgconfig/libgum.pc
%if %{dbus_type} != "p2p"
%{_datadir}/dbus-1/interfaces/*UserManagement*.xml
%endif


%files
%defattr(-,root,root,-)
%doc AUTHORS COPYING.LIB INSTALL NEWS README
%{_bindir}/%{name}
%config(noreplace) %{_sysconfdir}/gumd.conf
%if %{dbus_type} == "session"
%dir %{_datadir}/dbus-1/services
%{_datadir}/dbus-1/services/*UserManagement*.service
%else if %{dbus_type} == "system"
%dir %{_datadir}/dbus-1/system-services
%{_datadir}/dbus-1/system-services/*UserManagement*.service
%dir %{_sysconfdir}/dbus-1
%dir %{_sysconfdir}/dbus-1/system.d
%config(noreplace) %{_sysconfdir}/dbus-1/system.d/gumd-dbus.conf
%endif


%files doc
%defattr(-,root,root,-)
%{_datadir}/gtk-doc/html/gumd/*


%changelog
* Tue Aug 05 2014 Imran Zaman <imran.zaman@intel.com>
- Log only when logging is enabled
- Fixed p2p stream descriptor leak

* Thu May 22 2014 Imran Zaman <imran.zaman@intel.com>
- Renamed gum-example as gum-utils to be used as command line utility

* Wed May 21 2014 Imran Zaman <imran.zaman@intel.com>
- Fixed Bug # TIVI-2988
- Updated logs

* Tue May 20 2014 Imran Zaman <imran.zaman@intel.com>
- Fixed bug#TIVI-3170 (fixed smack labels for newly created files and folders
  as reported in https://bugs.tizen.org/jira/browse/TIVI-3170)

* Wed Feb 12 2014 Imran Zaman <imran.zaman@intel.com>
- Simplified gumd packages

* Mon Dec 23 2013 Imran Zaman <imran.zaman@intel.com>
- added test cases for error and dictionary objects
- utilized dictionary functions for get/set key-value pairs
- clean up generated coverage files on make clean
- enable tests by default if coverage is enabled
- exclude external and generated dbus files from code coverage calculation

* Fri Dec 20 2013 Imran Zaman <imran.zaman@intel.com>
- Corrected spec and changes file names 

* Fri Dec 20 2013 Imran Zaman <imran.zaman@intel.com>
- Removed dist spec packaging folder from main source tree

* Fri Dec 20 2013 Imran Zaman <imran.zaman@intel.com>
- Release 0.0.1 (First release)

* Mon Sep 02 2013 Imran Zaman <imran.zaman@intel.com>
- Initial RPM packaging
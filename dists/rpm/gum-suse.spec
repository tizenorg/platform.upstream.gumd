# define used dbus type [p2p, session, system]
%define dbus_type system
# enable debug features such as control environment variables
# WARNING! do not use for production builds as it will break security
%define debug_build 0


Name: gumd
Summary: User management daemon and client library
Version: 0.0.1
Release: 1
Group: System/Libraries
License: LGPL-2.1+
Source: %{name}-%{version}.tar.gz
URL: https://github.com/01org/gumd
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


%package -n libgum-common
Summary:    User management common library
Group:      System/Libraries


%description -n libgum-common
%{summary}.


%package -n libgum-common-devel
Summary:    Development files for user management common library
Group:      Development/Libraries
Requires:   libgum-common = %{version}-%{release}


%description -n libgum-common-devel
%{summary}.


%package %{name}
Summary:    User management daemon
Group:      System/Daemons
Requires:   libgum-common = %{version}-%{release}


%description %{name}
%{summary}.


%package -n %{name}-devel
Summary:    Development files for user management daemon
Group:      Development/Daemons
Requires:   %{name} = %{version}-%{release}
Requires:   libgum-common-devel = %{version}-%{release}

%description -n %{name}-devel
%{summary}.


%package -n libgum
Summary:    User management client library
Group:      System/Libraries
Requires:   libgum-common = %{version}-%{release}


%description -n libgum
%{summary}.


%package -n libgum-devel
Summary:    Development files for user management client library
Group:      Development/Libraries
Requires:   libgum = %{version}-%{release}
Requires:   libgum-common-devel = %{version}-%{release}


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


%files -n libgum-common
%defattr(-,root,root,-)
%{_libdir}/libgum-common*.so.*


%files -n libgum-common-devel
%defattr(-,root,root,-)
%{_includedir}/gum/common/*
%{_libdir}/libgum-common*.so
%{_libdir}/libgum-common*.la
%{_libdir}/pkgconfig/libgum-common.pc
%config(noreplace) %{_sysconfdir}/gum.conf
%if %{dbus_type} != "p2p"
%{_datadir}/dbus-1/interfaces/*UserManagement*.xml
%endif


%files %{name}
%defattr(-,root,root,-)
%doc AUTHORS COPYING.LIB INSTALL NEWS README
%{_bindir}/%{name}
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


%files -n %{name}-devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/%{name}.pc


%files -n libgum
%defattr(-,root,root,-)
%{_libdir}/libgum.so.*


%files -n libgum-devel
%defattr(-,root,root,-)
%{_includedir}/gum/*.h
%{_libdir}/libgum.so
%{_libdir}/libgum.la
%{_libdir}/pkgconfig/libgum.pc
%{_bindir}/gum-example


%files doc
%defattr(-,root,root,-)
%{_datadir}/gtk-doc/html/gumd/*


%changelog
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

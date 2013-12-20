# define used dbus type [p2p, session, system]
%define dbus_type system
# enable debug features such as control environment variables
# WARNING! do not use for production builds as it will break security
%define debug_build 0


Name: gum
Summary: User management daemon and client library
Version: 0.0.1
Release: 1
Group: System/Libraries
License: LGPL-2.1+
Source: %{name}d-%{version}.tar.gz
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


%package -n lib%{name}-common
Summary:    User management common library
Group:      System/Libraries


%description -n lib%{name}-common
%{summary}.


%package -n lib%{name}-common-devel
Summary:    Development files for user management common library
Group:      Development/Libraries
Requires:   lib%{name}-common = %{version}-%{release}


%description -n lib%{name}-common-devel
%{summary}.


%package -n %{name}d
Summary:    User management daemon
Group:      System/Daemons
Requires:   lib%{name}-common = %{version}-%{release}


%description -n %{name}d
%{summary}.


%package -n %{name}d-devel
Summary:    Development files for user management daemon
Group:      Development/Daemons
Requires:   %{name}d = %{version}-%{release}
Requires:   lib%{name}-common-devel = %{version}-%{release}

%description -n %{name}d-devel
%{summary}.


%package -n lib%{name}
Summary:    User management client library
Group:      System/Libraries
Requires:   lib%{name}-common = %{version}-%{release}


%description -n lib%{name}
%{summary}.


%package -n lib%{name}-devel
Summary:    Development files for user management client library
Group:      Development/Libraries
Requires:   lib%{name} = %{version}-%{release}
Requires:   lib%{name}-common-devel = %{version}-%{release}


%description -n lib%{name}-devel
%{summary}.


%package doc
Summary:    Documentation files for %{name}
Group:      Development/Libraries
Requires:   lib%{name} = %{version}-%{release}


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
chmod u+s %{_bindir}/%{name}d
groupadd -f -r gumd


%postun -p /sbin/ldconfig


%files -n lib%{name}-common
%defattr(-,root,root,-)
%{_libdir}/lib%{name}-common*.so.*


%files -n lib%{name}-common-devel
%defattr(-,root,root,-)
%{_includedir}/%{name}/common/*
%{_libdir}/lib%{name}-common*.so
%{_libdir}/lib%{name}-common*.la
%{_libdir}/pkgconfig/lib%{name}-common.pc
%config(noreplace) %{_sysconfdir}/gum.conf
%if %{dbus_type} != "p2p"
%{_datadir}/dbus-1/interfaces/*UserManagement*.xml
%endif


%files -n %{name}d
%defattr(-,root,root,-)
%doc AUTHORS COPYING.LIB INSTALL NEWS README
%{_bindir}/%{name}d
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


%files -n %{name}d-devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/%{name}d.pc


%files -n lib%{name}
%defattr(-,root,root,-)
%{_libdir}/lib%{name}.so.*


%files -n lib%{name}-devel
%defattr(-,root,root,-)
%{_includedir}/%{name}/*.h
%{_libdir}/lib%{name}.so
%{_libdir}/lib%{name}.la
%{_libdir}/pkgconfig/lib%{name}.pc
%{_bindir}/%{name}-example


%files doc
%defattr(-,root,root,-)
%{_datadir}/gtk-doc/html/gumd/*


%changelog
* Fri Dec 20 2013 Imran Zaman <imran.zaman@intel.com>
- Release 0.0.1 (First release)

* Mon Sep 02 2013 Imran Zaman <imran.zaman@intel.com>
- Initial RPM packaging

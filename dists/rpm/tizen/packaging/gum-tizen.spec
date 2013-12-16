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
Source: %{name}-%{version}.tar.gz
Source1001:     %{name}d.manifest
Source1002:     lib%{name}.manifest
Source1003:     lib%{name}-common.manifest
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


%prep
%setup -q -n %{name}-%{version}


%build
%if %{debug_build} == 1
%autogen --enable-dbus-type=%{dbus_type} %{_enable_debug}
%else
%autogen --enable-dbus-type=%{dbus_type}
%endif

make %{?_smp_mflags}


%install
rm -rf %{buildroot}
%make_install
cp -a %{SOURCE1001} %{buildroot}%{_datadir}/%{name}d.manifest
cp -a %{SOURCE1002} %{buildroot}%{_datadir}/lib%{name}.manifest
cp -a %{SOURCE1003} %{buildroot}%{_datadir}/lib%{name}-common.manifest

%post
/sbin/ldconfig
chmod u+s %{_bindir}/%{name}d
getent group gumd > /dev/null || /usr/sbin/groupadd -r gumd


%postun -p /sbin/ldconfig


%files -n lib%{name}-common
%defattr(-,root,root,-)
%manifest %{_datadir}/lib%{name}-common.manifest
%{_libdir}/lib%{name}-common*.so.*


%files -n lib%{name}-common-devel
%defattr(-,root,root,-)
%{_includedir}/%{name}/common/*
%{_libdir}/lib%{name}-common*.so
%{_libdir}/pkgconfig/lib%{name}-common.pc
%config(noreplace) %{_sysconfdir}/gum.conf
%if %{dbus_type} != "p2p"
%{_datadir}/dbus-1/interfaces/*UserManagement*.xml
%endif


%files -n %{name}d
%defattr(-,root,root,-)
%manifest %{_datadir}/%{name}d.manifest
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
%manifest %{_datadir}/lib%{name}.manifest
%{_libdir}/lib%{name}.so.*

%files -n lib%{name}-devel
%defattr(-,root,root,-)
%{_includedir}/%{name}/*.h
%{_libdir}/lib%{name}.so
%{_libdir}/pkgconfig/lib%{name}.pc
%{_bindir}/%{name}-example


# define used dbus type [p2p, system]
%define dbus_type system
# enable debug features such as control environment variables
# WARNING! do not use for production builds as it will break security
%define debug_build 0

%if "%{profile}" == "wearable"
%define disable_cap_admin 1
%else
%define disable_cap_admin 0
%endif


Name:    gumd
Summary: User management daemon and client library
Version: 1.0.8
Release: 0
Group:   Security/Accounts
License: LGPL-2.1+ and GPL-3.0+ and GPL-3.0-with-autoconf-exception
URL:     https://github.com/01org/gumd
Source:  %{name}-%{version}.tar.gz
Source1001:     %{name}.manifest
Source1002:     libgum.manifest
Requires:       libgum = %{version}-%{release}
Conflicts: gum
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
BuildRequires: pkgconfig(libtzplatform-config)
Requires: tizen-platform-config

%description
%{summary} files


%package -n libgum
Summary:    User management client library
Group:      Security/Libraries
Requires:   %{name} = %{version}-%{release}


%description -n libgum
%{summary} files


%package -n gum-utils
Summary:    User management utility tool
Group:      Security/Libraries
Requires:   libgum = %{version}-%{release}


%description -n gum-utils
%{summary} files


%package -n libgum-devel
Summary:    Development files for user management client library
Group:      Security/Libraries
Requires:   libgum = %{version}-%{release}


%description -n libgum-devel
%{summary} files


%package doc
Summary:    Documentation files for %{name}
Group:      Security/Documentation
Requires:   libgum = %{version}-%{release}


%description doc
%{summary} files


%prep
%setup -q -n %{name}-%{version}
cp -a %{SOURCE1001} %{name}.manifest
cp -a %{SOURCE1002} libgum.manifest
%if %{disable_cap_admin} == 1
echo "CapabilityBoundingSet=~CAP_MAC_ADMIN" >> data/gumd.service
%endif

%build
autoreconf -ivf
%if %{debug_build} == 1
%configure --enable-dbus-type=%{dbus_type} --enable-debug
%else
%configure --enable-dbus-type=%{dbus_type}
%endif
%__make %{?_smp_mflags}

%install
rm -rf %{buildroot}
%make_install
rm -f %{buildroot}%{_sysconfdir}/%{name}/%{name}.conf
install -m 755 -d %{buildroot}%{_sysconfdir}/%{name}
install -m 644 data/tizen/etc/%{name}/%{name}-tizen-common.conf %{buildroot}%{_sysconfdir}/%{name}/%{name}.conf
install -m 755 -d %{buildroot}%{_unitdir}
install -m 644 data/gumd.service %{buildroot}%{_unitdir}

%post
ldconfig
getent group gumd > /dev/null || groupadd -r gumd
install -d -m 755 %{_sysconfdir}/%{name}/useradd.d
install -d -m 755 %{_sysconfdir}/%{name}/userdel.d
install -d -m 755 %{_sysconfdir}/%{name}/groupadd.d
install -d -m 755 %{_sysconfdir}/%{name}/groupdel.d
install -d -m 755 %{_localstatedir}/lib/%{name}/user


%postun -p /sbin/ldconfig

%post -n libgum -p /sbin/ldconfig
%postun -n libgum -p /sbin/ldconfig

%files -n libgum
%defattr(-,root,root,-)
%manifest libgum.manifest
%{_libdir}/libgum*.so.*

%files -n gum-utils
%defattr(-,root,root,-)
%manifest %{name}.manifest
%{_bindir}/gum-utils

%files -n libgum-devel
%defattr(-,root,root,-)
%manifest %{name}.manifest
%{_includedir}/gum/*
%{_libdir}/libgum*.so
%{_libdir}/pkgconfig/libgum.pc
%if %{dbus_type} != "p2p"
%{_datadir}/dbus-1/interfaces/*UserManagement*.xml
%endif

%files
%defattr(-,root,root,-)
%manifest %{name}.manifest
%doc AUTHORS COPYING.LIB NEWS README
%{_bindir}/%{name}
%config(noreplace) %{_sysconfdir}/%{name}/%{name}.conf
%if %{dbus_type} == "system"
%dir %{_datadir}/dbus-1/system-services
%{_datadir}/dbus-1/system-services/*UserManagement*.service
%dir %{_sysconfdir}/dbus-1
%dir %{_sysconfdir}/dbus-1/system.d
%config(noreplace) %{_sysconfdir}/dbus-1/system.d/gumd-dbus.conf
%{_unitdir}/gumd.service
%endif

%files doc
%defattr(-,root,root,-)
%manifest %{name}.manifest
%{_datadir}/gtk-doc/html/gumd/*

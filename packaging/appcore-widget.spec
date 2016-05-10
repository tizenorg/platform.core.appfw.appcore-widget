Name:       appcore-widget
Summary:    Widget Application
Version:    1.0.0.0
Release:    1
Group:      Application Framework/Libraries
License:    Apache-2.0
Source0:    appcore-widget-%{version}.tar.gz
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(vconf-internal-keys)
BuildRequires:  pkgconfig(alarm-service)
BuildRequires:  pkgconfig(capi-appfw-app-control)
BuildRequires:  pkgconfig(appcore-common)
BuildRequires:  pkgconfig(capi-appfw-app-common)
BuildRequires:  pkgconfig(widget_service)
BuildRequires:  pkgconfig(capi-system-info)
BuildRequires:  cmake


%description
Widget application

%package -n capi-appfw-widget-application-devel
Summary:    Widget application
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}
%description -n capi-appfw-widget-application-devel
widget application (development files)

%prep
%setup -q

%build
MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`
%cmake . -DFULLVER=%{version} -DMAJORVER=${MAJORVER}
%__make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}%{_libdir}/pkgconfig
cp capi-appfw-widget-application.pc %{buildroot}%{_libdir}/pkgconfig
mkdir -p %{buildroot}/usr/share/license
cp LICENSE %{buildroot}/usr/share/license/%{name}


%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%manifest appcore-widget.manifest
%defattr(-,root,root,-)
%{_libdir}/libcapi-appfw-widget-application.so
%{_libdir}/libcapi-appfw-widget-application.so.1
%{_libdir}/libcapi-appfw-widget-application.so.1.1
/usr/share/license/%{name}

%files -n capi-appfw-widget-application-devel
/usr/include/appfw/widget_app.h
/usr/include/appfw/widget_app_efl.h
/usr/include/appfw/widget_app_internal.h
%{_libdir}/pkgconfig/capi-appfw-widget-application.pc


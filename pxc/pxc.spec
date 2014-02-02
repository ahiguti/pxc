%global __os_install_post %{nil}

Summary: P Extension Compiler
Name: pxc
Version: 0.0.3
Release: 1%{?dist}
Group: System Environment/Libraries
License: BSD
Source: %{name}.tar.gz
Packager: Akira Higuchi ( https://github.com/ahiguti )
BuildRoot: /var/tmp/%{name}-%{version}-root
BuildRequires: gcc-c++, boost-devel
Requires: gcc-c++, boost-devel

%description

%prep
%setup -n %{name}

# %define _use_internal_dependency_generator 0

%build
make pxc

%install
rm -rf $RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/%{_sysconfdir}
mkdir -p $RPM_BUILD_ROOT/%{_bindir}
mkdir -p $RPM_BUILD_ROOT/usr/share
install -m 644 pxc.profile $RPM_BUILD_ROOT/%{_sysconfdir}/
install -m 644 pxc_unsafe.profile $RPM_BUILD_ROOT/%{_sysconfdir}/
install -m 644 pxc_dynamic.profile $RPM_BUILD_ROOT/%{_sysconfdir}/
install -m 755 pxc $RPM_BUILD_ROOT/%{_bindir}/
cp -a libs/pxc_* $RPM_BUILD_ROOT/usr/share/

%files
%defattr(-, root, root)
%config(noreplace,missingok) %{_sysconfdir}/pxc.profile
%config(noreplace,missingok) %{_sysconfdir}/pxc_unsafe.profile
%config(noreplace,missingok) %{_sysconfdir}/pxc_dynamic.profile
%{_bindir}/pxc
/usr/share/pxc_*


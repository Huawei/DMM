Name:           dmm
Version:        18.04
Release:        1%{?dist}
Summary:        DMM Project

License:        GPL
URL:            https://gerrit.fd.io/r/dmm
Source:         %{name}-%{version}.tar.gz

BuildRequires: glibc, libstdc++, libgcc, numactl-libs
BuildRequires: dpdk >= 18.02, dpdk-devel >= 18.02

%description
The DMM framework provides posix socket APIs to the application. A protocol
stack could be plugged into the DMM. DMM will choose the most suitable stack
for the application.

%prep
%setup -q


%build
cd build/
cmake ..
make -j 8

%install
cd ../../BUILDROOT
mkdir -p %{name}-%{version}-%{release}.x86_64/usr/bin
mkdir -p %{name}-%{version}-%{release}.x86_64/usr/lib64

install -c ../BUILD/%{name}-%{version}/release/bin/kc_common %{name}-%{version}-%{release}.x86_64/usr/bin
install -c ../BUILD/%{name}-%{version}/release/bin/ks_epoll %{name}-%{version}-%{release}.x86_64/usr/bin
install -c ../BUILD/%{name}-%{version}/release/bin/vc_common %{name}-%{version}-%{release}.x86_64/usr/bin
install -c ../BUILD/%{name}-%{version}/release/bin/vs_epoll %{name}-%{version}-%{release}.x86_64/usr/bin
install -c ../BUILD/%{name}-%{version}/release/bin/ks_select %{name}-%{version}-%{release}.x86_64/usr/bin
install -c ../BUILD/%{name}-%{version}/release/bin/vs_select %{name}-%{version}-%{release}.x86_64/usr/bin
install -c ../BUILD/%{name}-%{version}/release/bin/ks_common %{name}-%{version}-%{release}.x86_64/usr/bin
install -c ../BUILD/%{name}-%{version}/release/bin/vs_common %{name}-%{version}-%{release}.x86_64/usr/bin

install -c ../BUILD/%{name}-%{version}/release/lib64/libdmm_api.so %{name}-%{version}-%{release}.x86_64/usr/lib64
install -c ../BUILD/%{name}-%{version}/release/lib64/libnStackAPI.so %{name}-%{version}-%{release}.x86_64/usr/lib64

%files
/usr/bin/*
/usr/lib64/*
%doc


%changelog

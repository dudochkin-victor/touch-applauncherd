# 
# Do NOT Edit the Auto-generated Part!
# Generated by: spectacle version 0.21
# 
# >> macros
# << macros

Name:       meegotouch-applauncherd
Summary:    Application launcher for fast startup
Version:    0.18.1
Release:    1
Group:      System/Daemons
License:    LGPLv2+
URL:        http://meego.gitorious.com/meegotouch/meegotouch-applauncherd
Source0:    %{name}-%{version}.tar.bz2
Source100:  meegotouch-applauncherd.yaml
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(QtCore)
BuildRequires:  pkgconfig(meegotouch)
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(xtst)
BuildRequires:  pkgconfig(xextproto)
BuildRequires:  pkgconfig(xi)
BuildRequires:  pkgconfig(xext)
BuildRequires:  cmake


%description
Application invoker and launcher daemon that speed up
application startup time and share memory. Provides also
functionality to launch applications as single instances.



%package devel
Summary:    Development files for launchable applications
Group:      Development/Tools
Requires:   %{name} = %{version}-%{release}

%description devel
Development files for creating applications that can be launched 
using meegotouch-applauncherd.


%package doc
Summary:    Instructions for application developer
Group:      Development/Tools
Requires:   %{name} = %{version}-%{release}

%description doc
Documentation files for application developer. 


%package testapps
Summary:    Test applications for launchable applications
Group:      Development/Tools
Requires:   %{name} = %{version}-%{release}

%description testapps
Test applications used for testing meegotouch-applauncherd.


%package tests
Summary:    Test scripts for launchable applications
Group:      Development/Tools
Requires:   %{name} = %{version}-%{release}
Requires:   %{name}-testapps = %{version}-%{release}
BuildRequires:   desktop-file-utils

%description tests
Test scripts used for testing meegotouch-applauncherd.



%prep
%setup -q -n %{name}-%{version}

# >> setup
# << setup

%build
# >> build pre
export BUILD_TESTS=1
export MEEGO=1
unset LD_AS_NEEDED
# << build pre

%configure --disable-static
make %{?jobs:-j%jobs}

# >> build post
# << build post
%install
rm -rf %{buildroot}
# >> install pre
# << install pre
%make_install

# >> install post
# rpmlint complains about installing binaries in /usr/share, so
# move them elsewhere and leave a symlink in place.
mv %{buildroot}/usr/share/applauncherd-tests %{buildroot}/usr/lib
(cd %{buildroot}/usr/share; ln -s ../lib/applauncherd-tests)
# << install post



%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig














%files
%defattr(-,root,root,-)
%{_bindir}/invoker
%{_libdir}/applauncherd/libapplauncherd.so
%{_libdir}/applauncherd/libmbooster.so
%{_libdir}/applauncherd/libqtbooster.so
%{_libdir}/applauncherd/libqdeclarativebooster.so
%{_libdir}/libmdeclarativecache.so
%{_libdir}/libmdeclarativecache.so.0
%{_libdir}/libmdeclarativecache.so.0.1
%{_bindir}/applauncherd.bin
%{_bindir}/applauncherd
%{_bindir}/single-instance
%config %{_sysconfdir}/xdg/autostart/applauncherd.desktop
# >> files
# << files


%files devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/meegotouch-boostable.pc
%{_libdir}/pkgconfig/qdeclarative-boostable.pc
%{_libdir}/pkgconfig/qt-boostable.pc
%{_includedir}/applauncherd/mdeclarativecache.h
%{_datadir}/qt4/mkspecs/features/meegotouch-boostable.prf
%{_datadir}/qt4/mkspecs/features/qt-boostable.prf
%{_datadir}/qt4/mkspecs/features/qdeclarative-boostable.prf
# >> files devel
# << files devel

%files doc
%defattr(-,root,root,-)
%doc %{_docdir}/applauncherd/README
%doc %{_docdir}/applauncherd/README-QDECLARATIVEBOOSTER
%doc %{_docdir}/applauncherd/examples/boosted-qml/with-cmake/CMakeLists.txt
%doc %{_docdir}/applauncherd/examples/boosted-qml/with-cmake/README
%doc %{_docdir}/applauncherd/examples/boosted-qml/with-cmake/main.cpp
%doc %{_docdir}/applauncherd/examples/boosted-qml/with-cmake/main.qml
%doc %{_docdir}/applauncherd/examples/boosted-qml/with-qmake/README
%doc %{_docdir}/applauncherd/examples/boosted-qml/with-qmake/main.cpp
%doc %{_docdir}/applauncherd/examples/boosted-qml/with-qmake/main.qml
%doc %{_docdir}/applauncherd/examples/boosted-qml/with-qmake/with-qmake.pro
# >> files doc
# << files doc

%files testapps
%defattr(-,root,root,-)
%{_bindir}/fala_ft_hello
%{_bindir}/fala_gettime_ms
%{_bindir}/fala_pixelchanged
%{_bindir}/fala_wl
%{_bindir}/fala_wl.launch
%{_bindir}/fala_wol
%{_bindir}/fala_wol.sh
%{_bindir}/fala_gettime
%{_bindir}/fala_status.launch
%{_bindir}/fala_ft_hello1
%{_bindir}/fala_ft_hello2
%{_bindir}/fala_ft_hello.launch
%{_bindir}/fala_ft_hello1.launch
%{_bindir}/fala_ft_hello2.launch
%{_bindir}/fala_testapp
%{_bindir}/fala_ft_themetest.launch
%{_bindir}/fala_ft_themetest
%{_bindir}/fala_windowid
%{_bindir}/fala_multi-instance
%{_bindir}/fala_qml_helloworld
%{_bindir}/fala_qml_helloworld.launch
%{_bindir}/fala_qml_helloworld1
%{_bindir}/fala_qml_helloworld1.launch
%{_bindir}/fala_qml_helloworld2
%{_bindir}/fala_qml_helloworld2.launch
%{_bindir}/fala_qml_wl
%{_bindir}/fala_qml_wl.launch
%{_bindir}/fala_qml_wol
%{_bindir}/xsendevent
%{_datadir}/themes/base/meegotouch/fala_ft_themetest/svg/baa.svg
%{_datadir}/dbus-1/services/com.nokia.fala_testapp.service
# >> files testapps
# << files testapps

%files tests
%defattr(-,root,root,-)
%{_datadir}/applauncherd-M-art-tests/tests.xml
%{_datadir}/applauncherd-M-bug-tests/tests.xml
%{_datadir}/applauncherd-M-functional-tests/tests.xml
%{_datadir}/applauncherd-M-performance-tests/tests.xml
%{_datadir}/applauncherd-tests
%{_libdir}/applauncherd-tests/tests.xml
%{_libdir}/applauncherd-tests/ut_booster
%{_libdir}/applauncherd-tests/ut_boosterfactory
%{_libdir}/applauncherd-tests/ut_daemon
%{_libdir}/applauncherd-tests/ut_connection
%{_libdir}/applauncherd-tests/ut_mbooster
%{_libdir}/applauncherd-tests/ut_qtbooster
%{_libdir}/applauncherd-tests/ut_socketmanager
%{_libdir}/applauncherd-tests/ut_dbooster
%{_datadir}/applauncherd-M-testscripts/check_pipes.py
%{_datadir}/applauncherd-M-testscripts/signal-forward/fala_sf_m.py
%{_datadir}/applauncherd-M-testscripts/signal-forward/fala_sf_m.sh
%{_datadir}/applauncherd-M-testscripts/signal-forward/fala_sf_qt.py
%{_datadir}/applauncherd-M-testscripts/signal-forward/fala_sf_qt.sh
%{_datadir}/applauncherd-M-testscripts/tc_theming.rb
%{_datadir}/applauncherd-M-testscripts/test-func-launcher.py
%{_datadir}/applauncherd-M-testscripts/test-perf-mbooster.py
%{_datadir}/applauncherd-M-testscripts/ts_prestartapp.rb
%{_datadir}/applauncherd-M-testscripts/fala_wid
%{_datadir}/applauncherd-M-testscripts/fala_xres_wl
%{_datadir}/applauncherd-M-testscripts/fala_xres_wol
%{_datadir}/applauncherd-M-testscripts/test-perf.rb
%{_datadir}/applauncherd-M-testscripts/utils.py
%{_datadir}/themes/base/meegotouch/fala_ft_themetest/style/fala_ft_themetest.css
%{_datadir}/applications/fala_wl.desktop
%{_datadir}/applications/fala_wol.desktop
%{_datadir}/applications/fala_qml_wol.desktop
%{_datadir}/applications/fala_qml_wl.desktop
%{_datadir}/fala_qml_helloworld/main.qml
%{_datadir}/dbus-1/services/com.nokia.fala_wl.service
%{_datadir}/dbus-1/services/com.nokia.fala_wol.service
# >> files tests
# << files tests


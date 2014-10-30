Name:       connectionagent-qt5

Summary:    User Agent daemon
Version:    0.11.25
Release:    0
Group:      Communications/Connectivity Adaptation
License:    LGPLv2
URL:        http://github.com/lpotter/connectionagent
Source0:    %{name}-%{version}.tar.bz2
Source1:    connectionagent.tracing
Requires:   connman-qt5-declarative
Requires:   systemd
Requires:   systemd-user-session-targets
Requires: connman >= 1.21
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(connman-qt5)
BuildRequires:  pkgconfig(qofono-qt5)
BuildRequires:  pkgconfig(Qt5Network)
BuildRequires:  pkgconfig(Qt5Test)
BuildRequires:  pkgconfig(Qt5Qml)
BuildRequires:  pkgconfig(qt5-boostable)
Provides:   connectionagent > 0.10.1
Obsoletes:   connectionagent <= 0.7.6

%description
Connection Agent provides multi user access to connman's User Agent.
It also provides autoconnecting features.

%package declarative
Summary:    Declarative plugin for connection agent.
Group:      Development/Tools
Requires:   %{name} = %{version}-%{release}
Requires:   %{name} = %{version}

%description declarative
This package contains the declarative plugin for connection agent.

%package test
Summary:    auto test for connection agent.
Group:      Development/Tools
Requires:   %{name} = %{version}-%{release}
Requires:   %{name} = %{version}

%description test
This package contains the auto tests for connection agent.

%package tracing
Summary:    Configuration for Connectionagent to enable tracing
Group:      Development/Tools
Requires:   %{name} = %{version}-%{release}

%description tracing
Will enable tracing for Connectionagent

%prep
%setup -q -n %{name}-%{version}

%build
%{!?qtc_qmake5:%define qtc_qmake5 %qmake5}
%{!?qtc_make:%define qtc_make make}

%qtc_qmake5
%qtc_make %{?_smp_mflags}


%install
rm -rf %{buildroot}
%qmake5_install

%make_install
mkdir -p %{buildroot}%{_sysconfdir}/tracing/connectionagent/
cp -a %{SOURCE1} %{buildroot}%{_sysconfdir}/tracing/connectionagent/

mkdir -p %{buildroot}%{_libdir}/systemd/user/user-session.target.wants
ln -s ../connectionagent.service %{buildroot}%{_libdir}/systemd/user/user-session.target.wants/

%post
if [ "$1" -ge 1 ]; then
systemctl-user daemon-reload || :
systemctl-user restart connectionagent.service || :
fi

%postun
if [ "$1" -eq 0 ]; then
systemctl-user stop connectionagent.service || :
systemctl-user daemon-reload || :
fi

%files
%defattr(-,root,root,-)
%{_bindir}/connectionagent
%{_bindir}/connectionagent-wrapper
%{_datadir}/dbus-1/services/com.jolla.Connectiond.service
%{_libdir}/systemd/user/connectionagent.service
%{_sysconfdir}/dbus-1/session.d/connectionagent.conf
%{_libdir}/systemd/user/user-session.target.wants/connectionagent.service

%files declarative
%defattr(-,root,root,-)
%{_libdir}/qt5/qml/com/jolla/connection/*

%files test
%defattr(-,root,root,-)
%{_prefix}/opt/tests/libqofono/*

%files tracing
%defattr(-,root,root,-)
%config %{_sysconfdir}/tracing/connectionagent

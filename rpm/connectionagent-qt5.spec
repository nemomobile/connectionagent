# 
# Do NOT Edit the Auto-generated Part!
# Generated by: spectacle version 0.26
# 

Name:       connectionagent-qt5

# >> macros
# << macros

Summary:    User Agent daemon
Version:    0.6.8
Release:    0
Group:      Communications/Connectivity Adaptation
License:    LGPLv2
URL:        http://github.com/lpotter/connectionagent
Source0:    %{name}-%{version}.tar.bz2
Source100:  connectionagent-qt5.yaml
Requires:   connman-qt5-declarative
BuildRequires:  pkgconfig(Qt5Core)
BuildRequires:  pkgconfig(Qt5DBus)
BuildRequires:  pkgconfig(connman-qt5)

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
Summary:    Declarative test for connection agent.
Group:      Development/Tools
Requires:   %{name} = %{version}-%{release}
Requires:   %{name} = %{version}

%description test
This package contains the declarative plugin for connection agent.

%prep
%setup -q -n %{name}-%{version}

# >> setup
# << setup

%build
# >> build pre
# << build pre

%qmake5 

make %{?jobs:-j%jobs}

# >> build post
# << build post

%install
rm -rf %{buildroot}
# >> install pre
# << install pre
%qmake_install

# >> install post
# << install post

%files
%defattr(-,root,root,-)
%{_bindir}/connectionagent
%{_datadir}/dbus-1/services/com.jolla.Connectiond.service
%{_libdir}/systemd/user/connectionagent.service
# >> files
# << files

%files declarative
%defattr(-,root,root,-)
%{_libdir}/qt5/imports/com/jolla/connection/*
# >> files declarative
# << files declarative

%files test
%defattr(-,root,root,-)
/opt/sdk/bin/testqml
/opt/sdk/share/applications/testqml.desktop
/opt/sdk/share/testqml/*
# >> files test
# << files test

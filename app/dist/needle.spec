# needle's best-effort .rpm (README, "Other distributions"). app/dist/build-rpm builds needle and stages it;
# this spec only packages the staged files, so rpmbuild works out the library dependencies itself.
%global debug_package %{nil}

Name:           needle
Version:        %{needle_version}
Release:        1
Summary:        Review Spotify discoveries and likes, with haystack
License:        MIT AND OFL-1.1
URL:            https://github.com/kurtlieber/needle
# Loaded at run time, so rpmbuild can't see them
Requires:       qt6-qtdeclarative qt6-qtwayland xdg-utils

%description
needle plays the songs haystack finds for you, shows how each one sounds against what you usually like, and
sends your ratings back to haystack. Best effort: Arch Linux (Omarchy) is the supported platform.

%install
cp -a %{needle_stage}/. %{buildroot}/

%files
%license /usr/share/licenses/needle
/usr/bin/needle
/usr/lib/needle
/usr/share/applications/needle.desktop
/usr/share/icons/hicolor/scalable/apps/needle.svg

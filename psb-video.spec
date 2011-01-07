%define psb_video_dir	/mnt/work/mrstddk-m2-mdfld/usr/src/meego/psb-video
%define local_so_dir	%{psb_video_dir}/src/.libs
%define local_bin_dir	%{psb_video_dir}/fw

Summary:	Poulsbo Video Drivers
Name:           psb-video
Version:        1.0.6
Source:		%{name}-%{version}.tar.bz
Release:	0
License:        Intel Proprietary
Group:          Development
Summary:        User space video driver
BuildRoot:      %{_tmppath}/%{name}-%{version}-build

%description
User space video driver

%prep

%build

%install
rm -rf $RPM_BUILD_ROOT

mkdir -p $RPM_BUILD_ROOT%{_libdir}/firmware
mkdir -p $RPM_BUILD_ROOT%{_libdir}/dri
install -m 755 %{local_so_dir}/pvr_drv_video.so $RPM_BUILD_ROOT%{_libdir}/dri/pvr_drv_video.so
install -m 755 %{local_so_dir}/pvr_drv_video.la $RPM_BUILD_ROOT%{_libdir}/dri/pvr_drv_video.la
install -m 755 %{local_bin_dir}/msvdx_fw.bin $RPM_BUILD_ROOT%{_libdir}/firmware/msvdx_fw.bin
install -m 755 %{local_bin_dir}/msvdx_fw_mfld.bin $RPM_BUILD_ROOT%{_libdir}/firmware/msvdx_fw_mfld.bin
install -m 755 %{local_bin_dir}/msvdx_fw_mfld_DE2.0.bin $RPM_BUILD_ROOT%{_libdir}/firmware/msvdx_fw_mfld_DE2.0.bin
install -m 755 %{local_bin_dir}/topaz_fw.bin $RPM_BUILD_ROOT%{_libdir}/firmware/topaz_fw.bin
install -m 755 %{local_bin_dir}/topazsc_fw.bin $RPM_BUILD_ROOT%{_libdir}/firmware/topazsc_fw.bin

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
%{_libdir}/dri/pvr_drv_video.so
%{_libdir}/dri/pvr_drv_video.la
%{_libdir}/firmware/msvdx_fw.bin
%{_libdir}/firmware/msvdx_fw_mfld.bin
%{_libdir}/firmware/msvdx_fw_mfld_DE2.0.bin
%{_libdir}/firmware/topaz_fw.bin
%{_libdir}/firmware/topazsc_fw.bin

%changelog
* Thu Oct 21 2010 Zhaohan, Ren <zhaohan.ren@intel.com> 1.0.6
- Updated psb-video to version 1.0.6


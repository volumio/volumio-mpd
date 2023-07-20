import re
from os.path import abspath

from build.project import Project
from build.zlib import ZlibProject
from build.meson import MesonProject
from build.cmake import CmakeProject
from build.autotools import AutotoolsProject
from build.ffmpeg import FfmpegProject
from build.openssl import OpenSSLProject
from build.boost import BoostProject
from build.jack import JackProject

libmpdclient = MesonProject(
    'https://www.musicpd.org/download/libmpdclient/2/libmpdclient-2.19.tar.xz',
    '158aad4c2278ab08e76a3f2b0166c99b39fae00ee17231bd225c5a36e977a189',
    'lib/libmpdclient.a',
)

libogg = CmakeProject(
    'http://downloads.xiph.org/releases/ogg/libogg-1.3.4.tar.xz',
    'c163bc12bc300c401b6aa35907ac682671ea376f13ae0969a220f7ddf71893fe',
    'lib/libogg.a',
    [
        '-DBUILD_SHARED_LIBS=OFF',
        '-DINSTALL_DOCS=OFF',
        '-DINSTALL_CMAKE_PACKAGE_MODULE=OFF',
    ],
)

opus = AutotoolsProject(
    'https://archive.mozilla.org/pub/opus/opus-1.3.1.tar.gz',
    '65b58e1e25b2a114157014736a3d9dfeaad8d41be1c8179866f144a2fb44ff9d',
    'lib/libopus.a',
    [
        '--disable-shared', '--enable-static',
        '--disable-doc',
        '--disable-extra-programs',
    ],

    # suppress "visibility default" from opus_defines.h
    cppflags='-DOPUS_EXPORT=',
)

flac = AutotoolsProject(
    'http://downloads.xiph.org/releases/flac/flac-1.3.3.tar.xz',
    '213e82bd716c9de6db2f98bcadbc4c24c7e2efe8c75939a1a84e28539c4e1748',
    'lib/libFLAC.a',
    [
        '--disable-shared', '--enable-static',
        '--disable-xmms-plugin', '--disable-cpplibs',
        '--disable-doxygen-docs',
    ],
    subdirs=['include', 'src/libFLAC'],
)

zlib = ZlibProject(
    'http://zlib.net/zlib-1.2.11.tar.xz',
    '4ff941449631ace0d4d203e3483be9dbc9da454084111f97ea0a2114e19bf066',
    'lib/libz.a',
)

libid3tag = AutotoolsProject(
    'ftp://ftp.mars.org/pub/mpeg/libid3tag-0.15.1b.tar.gz',
    'e5808ad997ba32c498803822078748c3',
    'lib/libid3tag.a',
    [
        '--disable-shared', '--enable-static',

        # without this, libid3tag's configure.ac ignores -O* and -f*
        '--disable-debugging',
    ],
    autogen=True,

    edits={
        # fix bug in libid3tag's configure.ac which discards all but the last optimization flag
        'configure.ac': lambda data: re.sub(r'optimize="\$1"', r'optimize="$optimize $1"', data, count=1),
    }
)

libmad = AutotoolsProject(
    'ftp://ftp.mars.org/pub/mpeg/libmad-0.15.1b.tar.gz',
    '1be543bc30c56fb6bea1d7bf6a64e66c',
    'lib/libmad.a',
    [
        '--disable-shared', '--enable-static',

        # without this, libmad's configure.ac ignores -O* and -f*
        '--disable-debugging',
    ],
    autogen=True,
)

liblame = AutotoolsProject(
    'http://downloads.sourceforge.net/project/lame/lame/3.100/lame-3.100.tar.gz',
    'ddfe36cab873794038ae2c1210557ad34857a4b6bdc515785d1da9e175b1da1e',
    'lib/libmp3lame.a',
    [
        '--disable-shared', '--enable-static',
        '--disable-gtktest', '--disable-analyzer-hooks',
        '--disable-decoder', '--disable-frontend',
    ],
)

libmodplug = AutotoolsProject(
    'https://downloads.sourceforge.net/modplug-xmms/libmodplug/0.8.9.0/libmodplug-0.8.9.0.tar.gz',
    '457ca5a6c179656d66c01505c0d95fafaead4329b9dbaa0f997d00a3508ad9de',
    'lib/libmodplug.a',
    [
        '--disable-shared', '--enable-static',
    ],
)

libopenmpt = AutotoolsProject(
    'https://lib.openmpt.org/files/libopenmpt/src/libopenmpt-0.5.12+release.autotools.tar.gz',
    '892aea7a599b5d21842bebf463b5aafdad5711be7008dd84401920c6234820af',
    'lib/libopenmpt.a',
    [
        '--disable-shared', '--enable-static',
        '--disable-openmpt123',
        '--without-mpg123', '--without-ogg', '--without-vorbis', '--without-vorbisfile',
        '--without-portaudio', '--without-portaudiocpp', '--without-sndfile',
    ],
    base='libopenmpt-0.5.12+release.autotools',
)

wildmidi = CmakeProject(
    'https://codeload.github.com/Mindwerks/wildmidi/tar.gz/wildmidi-0.4.4',
    '6f267c8d331e9859906837e2c197093fddec31829d2ebf7b958cf6b7ae935430',
    'lib/libWildMidi.a',
    [
        '-DBUILD_SHARED_LIBS=OFF',
        '-DWANT_PLAYER=OFF',
        '-DWANT_STATIC=ON',
    ],
    base='wildmidi-wildmidi-0.4.4',
    name='wildmidi',
    version='0.4.4',
)

gme = CmakeProject(
    'https://bitbucket.org/mpyne/game-music-emu/downloads/game-music-emu-0.6.3.tar.xz',
    'aba34e53ef0ec6a34b58b84e28bf8cfbccee6585cebca25333604c35db3e051d',
    'lib/libgme.a',
    [
        '-DBUILD_SHARED_LIBS=OFF',
        '-DENABLE_UBSAN=OFF',
        '-DZLIB_INCLUDE_DIR=OFF',
        '-DSDL2_DIR=OFF',
    ],
)

ffmpeg = FfmpegProject(
    'http://ffmpeg.org/releases/ffmpeg-4.4.1.tar.xz',
    'eadbad9e9ab30b25f5520fbfde99fae4a92a1ae3c0257a8d68569a4651e30e02',
    'lib/libavcodec.a',
    [
        '--disable-shared', '--enable-static',
        '--enable-gpl',
        '--enable-small',
        '--disable-pthreads',
        '--disable-programs',
        '--disable-doc',
        '--disable-avdevice',
        '--disable-swresample',
        '--disable-swscale',
        '--disable-postproc',
        '--disable-avfilter',
        '--disable-lzo',
        '--disable-faan',
        '--disable-pixelutils',
        '--disable-network',
        '--disable-encoders',
        '--disable-muxers',
        '--disable-protocols',
        '--disable-devices',
        '--disable-filters',
        '--disable-v4l2_m2m',

        '--disable-parser=bmp',
        '--disable-parser=cavsvideo',
        '--disable-parser=dvbsub',
        '--disable-parser=dvdsub',
        '--disable-parser=dvd_nav',
        '--disable-parser=flac',
        '--disable-parser=g729',
        '--disable-parser=gsm',
        '--disable-parser=h261',
        '--disable-parser=h263',
        '--disable-parser=h264',
        '--disable-parser=hevc',
        '--disable-parser=mjpeg',
        '--disable-parser=mlp',
        '--disable-parser=mpeg4video',
        '--disable-parser=mpegvideo',
        '--disable-parser=opus',
        '--disable-parser=vc1',
        '--disable-parser=vp3',
        '--disable-parser=vp8',
        '--disable-parser=vp9',
        '--disable-parser=png',
        '--disable-parser=pnm',
        '--disable-parser=xma',

        '--disable-demuxer=aqtitle',
        '--disable-demuxer=ass',
        '--disable-demuxer=bethsoftvid',
        '--disable-demuxer=bink',
        '--disable-demuxer=cavsvideo',
        '--disable-demuxer=cdxl',
        '--disable-demuxer=dvbsub',
        '--disable-demuxer=dvbtxt',
        '--disable-demuxer=h261',
        '--disable-demuxer=h263',
        '--disable-demuxer=h264',
        '--disable-demuxer=ico',
        '--disable-demuxer=image2',
        '--disable-demuxer=jacosub',
        '--disable-demuxer=lrc',
        '--disable-demuxer=microdvd',
        '--disable-demuxer=mjpeg',
        '--disable-demuxer=mjpeg_2000',
        '--disable-demuxer=mpegps',
        '--disable-demuxer=mpegvideo',
        '--disable-demuxer=mpl2',
        '--disable-demuxer=mpsub',
        '--disable-demuxer=pjs',
        '--disable-demuxer=rawvideo',
        '--disable-demuxer=realtext',
        '--disable-demuxer=sami',
        '--disable-demuxer=scc',
        '--disable-demuxer=srt',
        '--disable-demuxer=stl',
        '--disable-demuxer=subviewer',
        '--disable-demuxer=subviewer1',
        '--disable-demuxer=swf',
        '--disable-demuxer=tedcaptions',
        '--disable-demuxer=vobsub',
        '--disable-demuxer=vplayer',
        '--disable-demuxer=webvtt',
        '--disable-demuxer=yuv4mpegpipe',

        # we don't need these decoders, because we have the dedicated
        # libraries
        '--disable-decoder=flac',
        '--disable-decoder=opus',
        '--disable-decoder=vorbis',

        # audio codecs nobody uses
        '--disable-decoder=atrac1',
        '--disable-decoder=atrac3',
        '--disable-decoder=atrac3al',
        '--disable-decoder=atrac3p',
        '--disable-decoder=atrac3pal',
        '--disable-decoder=binkaudio_dct',
        '--disable-decoder=binkaudio_rdft',
        '--disable-decoder=bmv_audio',
        '--disable-decoder=dsicinaudio',
        '--disable-decoder=dvaudio',
        '--disable-decoder=metasound',
        '--disable-decoder=paf_audio',
        '--disable-decoder=ra_144',
        '--disable-decoder=ra_288',
        '--disable-decoder=ralf',
        '--disable-decoder=qdm2',
        '--disable-decoder=qdmc',

        # disable lots of image and video codecs
        '--disable-decoder=ass',
        '--disable-decoder=asv1',
        '--disable-decoder=asv2',
        '--disable-decoder=apng',
        '--disable-decoder=avrn',
        '--disable-decoder=avrp',
        '--disable-decoder=bethsoftvid',
        '--disable-decoder=bink',
        '--disable-decoder=bmp',
        '--disable-decoder=bmv_video',
        '--disable-decoder=cavs',
        '--disable-decoder=ccaption',
        '--disable-decoder=cdgraphics',
        '--disable-decoder=clearvideo',
        '--disable-decoder=dirac',
        '--disable-decoder=dsicinvideo',
        '--disable-decoder=dvbsub',
        '--disable-decoder=dvdsub',
        '--disable-decoder=dvvideo',
        '--disable-decoder=exr',
        '--disable-decoder=ffv1',
        '--disable-decoder=ffvhuff',
        '--disable-decoder=ffwavesynth',
        '--disable-decoder=flic',
        '--disable-decoder=flv',
        '--disable-decoder=fraps',
        '--disable-decoder=gif',
        '--disable-decoder=h261',
        '--disable-decoder=h263',
        '--disable-decoder=h263i',
        '--disable-decoder=h263p',
        '--disable-decoder=h264',
        '--disable-decoder=hevc',
        '--disable-decoder=hnm4_video',
        '--disable-decoder=hq_hqa',
        '--disable-decoder=hqx',
        '--disable-decoder=idcin',
        '--disable-decoder=iff_ilbm',
        '--disable-decoder=indeo2',
        '--disable-decoder=indeo3',
        '--disable-decoder=indeo4',
        '--disable-decoder=indeo5',
        '--disable-decoder=interplay_video',
        '--disable-decoder=jacosub',
        '--disable-decoder=jpeg2000',
        '--disable-decoder=jpegls',
        '--disable-decoder=microdvd',
        '--disable-decoder=mimic',
        '--disable-decoder=mjpeg',
        '--disable-decoder=mmvideo',
        '--disable-decoder=mpl2',
        '--disable-decoder=motionpixels',
        '--disable-decoder=mpeg1video',
        '--disable-decoder=mpeg2video',
        '--disable-decoder=mpeg4',
        '--disable-decoder=mpegvideo',
        '--disable-decoder=mscc',
        '--disable-decoder=msmpeg4_crystalhd',
        '--disable-decoder=msmpeg4v1',
        '--disable-decoder=msmpeg4v2',
        '--disable-decoder=msmpeg4v3',
        '--disable-decoder=msvideo1',
        '--disable-decoder=mszh',
        '--disable-decoder=mvc1',
        '--disable-decoder=mvc2',
        '--disable-decoder=on2avc',
        '--disable-decoder=paf_video',
        '--disable-decoder=png',
        '--disable-decoder=qdraw',
        '--disable-decoder=qpeg',
        '--disable-decoder=rawvideo',
        '--disable-decoder=realtext',
        '--disable-decoder=roq',
        '--disable-decoder=roq_dpcm',
        '--disable-decoder=rscc',
        '--disable-decoder=rv10',
        '--disable-decoder=rv20',
        '--disable-decoder=rv30',
        '--disable-decoder=rv40',
        '--disable-decoder=sami',
        '--disable-decoder=sheervideo',
        '--disable-decoder=snow',
        '--disable-decoder=srt',
        '--disable-decoder=stl',
        '--disable-decoder=subrip',
        '--disable-decoder=subviewer',
        '--disable-decoder=subviewer1',
        '--disable-decoder=svq1',
        '--disable-decoder=svq3',
        '--disable-decoder=tiff',
        '--disable-decoder=tiertexseqvideo',
        '--disable-decoder=truemotion1',
        '--disable-decoder=truemotion2',
        '--disable-decoder=truemotion2rt',
        '--disable-decoder=twinvq',
        '--disable-decoder=utvideo',
        '--disable-decoder=vc1',
        '--disable-decoder=vmdvideo',
        '--disable-decoder=vp3',
        '--disable-decoder=vp5',
        '--disable-decoder=vp6',
        '--disable-decoder=vp7',
        '--disable-decoder=vp8',
        '--disable-decoder=vp9',
        '--disable-decoder=vqa',
        '--disable-decoder=webvtt',
        '--disable-decoder=wmv1',
        '--disable-decoder=wmv2',
        '--disable-decoder=wmv3',
        '--disable-decoder=yuv4',
    ],
)

openssl = OpenSSLProject(
    'https://www.openssl.org/source/openssl-3.0.0.tar.gz',
    '59eedfcb46c25214c9bd37ed6078297b4df01d012267fe9e9eee31f61bc70536',
    'include/openssl/ossl_typ.h',
)

curl = CmakeProject(
    'https://curl.se/download/curl-7.79.1.tar.xz',
    '0606f74b1182ab732a17c11613cbbaf7084f2e6cca432642d0e3ad7c224c3689',
    'lib/libcurl.a',
    [
        '-DBUILD_CURL_EXE=OFF',
        '-DBUILD_SHARED_LIBS=OFF',
        '-DCURL_DISABLE_VERBOSE_STRINGS=ON',
        '-DCURL_DISABLE_LDAP=ON',
        '-DCURL_DISABLE_TELNET=ON',
        '-DCURL_DISABLE_DICT=ON',
        '-DCURL_DISABLE_FILE=ON',
        '-DCURL_DISABLE_FTP=ON',
        '-DCURL_DISABLE_TFTP=ON',
        '-DCURL_DISABLE_LDAPS=ON',
        '-DCURL_DISABLE_RTSP=ON',
        '-DCURL_DISABLE_PROXY=ON',
        '-DCURL_DISABLE_POP3=ON',
        '-DCURL_DISABLE_IMAP=ON',
        '-DCURL_DISABLE_SMTP=ON',
        '-DCURL_DISABLE_GOPHER=ON',
        '-DCURL_DISABLE_COOKIES=ON',
        '-DCURL_DISABLE_CRYPTO_AUTH=ON',
        '-DCURL_DISABLE_ALTSVC=ON',
        '-DCMAKE_USE_LIBSSH2=OFF',
        '-DCURL_WINDOWS_SSPI=OFF',
        '-DCURL_DISABLE_NTLM=ON',
        '-DBUILD_TESTING=OFF',
    ],
    windows_configure_args=[
        '-DCMAKE_USE_SCHANNEL=ON',
    ],
    patches='src/lib/curl/patches',
)

libnfs = AutotoolsProject(
    'https://github.com/sahlberg/libnfs/archive/libnfs-4.0.0.tar.gz',
    '6ee77e9fe220e2d3e3b1f53cfea04fb319828cc7dbb97dd9df09e46e901d797d',
    'lib/libnfs.a',
    [
        '--disable-shared', '--enable-static',
        '--disable-debug',

        # work around -Wtautological-compare
        '--disable-werror',

        '--disable-utils', '--disable-examples',
    ],
    base='libnfs-libnfs-4.0.0',
    patches='src/lib/nfs/patches',
    autoreconf=True,
)

jack = JackProject(
    'https://github.com/jackaudio/jack2/archive/v1.9.17.tar.gz',
    '38f674bbc57852a8eb3d9faa1f96a0912d26f7d5df14c11005ad499c8ae352f2',
    'lib/pkgconfig/jack.pc',
)

boost = BoostProject(
    'https://boostorg.jfrog.io/artifactory/main/release/1.77.0/source/boost_1_77_0.tar.bz2',
    'fc9f85fc030e233142908241af7a846e60630aa7388de9a5fafb1f3a26840854',
    'include/boost/version.hpp',
)

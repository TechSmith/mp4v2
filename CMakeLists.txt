cmake_minimum_required(VERSION 3.6)
project(mp4v2_2_0_0)

set(CMAKE_CXX_STANDARD 11)

set(HEADER_FILES
        include/mp4v2/chapter.h
        include/mp4v2/file.h
        include/mp4v2/file_prop.h
        include/mp4v2/general.h
        include/mp4v2/isma.h
        include/mp4v2/itmf_generic.h
        include/mp4v2/itmf_tags.h
        include/mp4v2/mp4v2.h
        include/mp4v2/platform.h
        include/mp4v2/project.h
        include/mp4v2/sample.h
        include/mp4v2/streaming.h
        include/mp4v2/track.h
        include/mp4v2/track_prop.h
        libplatform/io/File.cpp
        libplatform/io/File.h
        libplatform/io/FileSystem.h
        libplatform/number/random.h
        libplatform/process/process.h
        libplatform/prog/option.h
        libplatform/sys/error.h
        libplatform/time/time.h
        libplatform/config.h
        libplatform/endian.h
        libplatform/impl.h
        libplatform/platform.h
        libplatform/platform_base.h
        libplatform/platform_posix.h
        libplatform/warning.h
        libutil/crc.h
        libutil/Database.h
        libutil/impl.h
        libutil/other.h
        libutil/Timecode.h
        libutil/TrackModifier.h
        libutil/util.h
        libutil/Utility.h
        src/bmff/bmff.h
        src/bmff/impl.h
        src/bmff/typebmff.h
        src/itmf/CoverArtBox.h
        #src/itmf/FreeformBox.h
        src/itmf/generic.h
        src/itmf/impl.h
        src/itmf/itmf.h
        src/itmf/Tags.h
        src/itmf/type.h
        src/qtff/coding.h
        src/qtff/ColorParameterBox.h
        src/qtff/impl.h
        src/qtff/PictureAspectRatioBox.h
        src/qtff/qtff.h
        src/atoms.h
        src/descriptors.h
        src/enum.h
        src/exception.h
        src/impl.h
        src/log.h
        src/mp4array.h
        src/mp4atom.h
        src/mp4container.h
        src/mp4descriptor.h
        src/mp4file.h
        src/mp4property.h
        src/mp4track.h
        src/mp4util.h
        src/ocidescriptors.h
        src/odcommands.h
        src/qosqualifiers.h
        src/rtphint.h
        src/src.h
        src/text.h
        src/util.h)

set(SOURCE_FILES
        libplatform/io/File_posix.cpp
        libplatform/io/FileSystem.cpp
        libplatform/io/FileSystem_posix.cpp
        libplatform/number/random_posix.cpp
        libplatform/process/process_posix.cpp
        libplatform/prog/option.cpp
        libplatform/sys/error.cpp
        libplatform/time/time.cpp
        libplatform/time/time_posix.cpp
        libutil/crc.cpp
        libutil/Database.cpp
        libutil/other.cpp
        libutil/Timecode.cpp
        libutil/TrackModifier.cpp
        libutil/Utility.cpp
        src/bmff/typebmff.cpp
        src/itmf/CoverArtBox.cpp
        #src/itmf/FreeformBox.cpp
        src/itmf/generic.cpp
        src/itmf/Tags.cpp
        src/itmf/type.cpp
        src/qtff/coding.cpp
        src/qtff/ColorParameterBox.cpp
        src/qtff/PictureAspectRatioBox.cpp
        src/3gp.cpp
        src/atom_ac3.cpp
        src/atom_amr.cpp
        src/atom_avc1.cpp
        src/atom_avcC.cpp
        src/atom_chpl.cpp
        src/atom_colr.cpp
        src/atom_d263.cpp
        src/atom_dac3.cpp
        src/atom_damr.cpp
        src/atom_dref.cpp
        src/atom_elst.cpp
        src/atom_enca.cpp
        src/atom_encv.cpp
        src/atom_free.cpp
        src/atom_ftab.cpp
        src/atom_ftyp.cpp
        src/atom_gmin.cpp
        src/atom_hdlr.cpp
        src/atom_hinf.cpp
        src/atom_hnti.cpp
        src/atom_href.cpp
        src/atom_mdat.cpp
        src/atom_mdhd.cpp
        src/atom_meta.cpp
        src/atom_mp4s.cpp
        src/atom_mp4v.cpp
        src/atom_mvhd.cpp
        src/atom_nmhd.cpp
        src/atom_ohdr.cpp
        src/atom_pasp.cpp
        src/atom_root.cpp
        src/atom_rtp.cpp
        src/atom_s263.cpp
        src/atom_sdp.cpp
        src/atom_sdtp.cpp
        src/atom_smi.cpp
        src/atom_sound.cpp
        src/atom_standard.cpp
        src/atom_stbl.cpp
        src/atom_stdp.cpp
        src/atom_stsc.cpp
        src/atom_stsd.cpp
        src/atom_stsz.cpp
        src/atom_stz2.cpp
        src/atom_text.cpp
        src/atom_tfhd.cpp
        src/atom_tkhd.cpp
        src/atom_treftype.cpp
        src/atom_trun.cpp
        src/atom_tx3g.cpp
        src/atom_udta.cpp
        src/atom_url.cpp
        src/atom_urn.cpp
        src/atom_uuid.cpp
        src/atom_video.cpp
        src/atom_vmhd.cpp
        src/cmeta.cpp
        src/descriptors.cpp
        src/enum.tcc
        src/exception.cpp
        src/isma.cpp
        src/log.cpp
        src/mp4.cpp
        src/mp4atom.cpp
        src/mp4container.cpp
        src/mp4descriptor.cpp
        src/mp4file.cpp
        src/mp4file_io.cpp
        src/mp4info.cpp
        src/mp4property.cpp
        src/mp4track.cpp
        src/mp4util.cpp
        src/ocidescriptors.cpp
        src/odcommands.cpp
        src/qosqualifiers.cpp
        src/rtphint.cpp
        src/text.cpp)

#add_library(mp4v2-shared SHARED ${HEADER_FILES} ${SOURCE_FILES})
# Just expose a static lib for now
add_library(mp4v2 STATIC ${HEADER_FILES} ${SOURCE_FILES})

target_include_directories(mp4v2 PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   )

#set(UTILITY_HEADERS
#        util/impl.h)
#
#add_executable(mp4art ${UTILITY_HEADERS} util/mp4art.cpp)
#target_link_libraries(mp4art mp4v2-static)
#
#add_executable(mp4chaps ${UTILITY_HEADERS} util/mp4chaps.cpp)
#target_link_libraries(mp4chaps mp4v2-static)
#
#add_executable(mp4extract ${UTILITY_HEADERS} util/mp4extract.cpp)
#target_link_libraries(mp4extract mp4v2-static)
#
#add_executable(mp4file ${UTILITY_HEADERS} util/mp4file.cpp)
#target_link_libraries(mp4file mp4v2-static)
#
#add_executable(mp4info ${UTILITY_HEADERS} util/mp4info.cpp)
#target_link_libraries(mp4info mp4v2-static)
#
#add_executable(mp4subtitle ${UTILITY_HEADERS} util/mp4subtitle.cpp)
#target_link_libraries(mp4subtitle mp4v2-static)
#
#add_executable(mp4tags ${UTILITY_HEADERS} util/mp4tags.cpp)
#target_link_libraries(mp4tags mp4v2-static)
#
#add_executable(mp4track ${UTILITY_HEADERS} util/mp4track.cpp)
#target_link_libraries(mp4track mp4v2-static)
#
#add_executable(mp4trackdump ${UTILITY_HEADERS} util/mp4trackdump.cpp)
#target_link_libraries(mp4trackdump mp4v2-static)
#
#
#
#
#
#
#
#
#
#

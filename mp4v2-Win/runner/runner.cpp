// runner.cpp : This file contains the 'main' function. Program execution begins and ends there.
#include <iostream>
#include <mp4v2/mp4v2.h>

#include <vector>

namespace
{
   bool parseAtomCallback( uint32_t fourCC )
   {
      static const std::vector<uint32_t> atomsToSkip = { 'udta', 'text' };

      return std::find( atomsToSkip.cbegin(), atomsToSkip.cend(), fourCC ) == atomsToSkip.cend();
   }

   bool isVideoTrack( MP4FileHandle fh, MP4TrackId trackId )
   {
      return strcmp( ::MP4GetTrackType( fh, trackId ), MP4_VIDEO_TRACK_TYPE ) == 0;
   }
}

std::vector<MP4TrackId> mp4TrackIdsOfAllVideoTracks( MP4FileHandle handle )
{
   std::vector<MP4TrackId> trackIds;
   uint32_t numTracks = ::MP4GetNumberOfTracks( handle );
   for ( uint32_t trackIndex = 0; trackIndex < numTracks; ++trackIndex )
   {
      MP4TrackId trackId = ::MP4FindTrackId( handle, (uint16_t)trackIndex );
      if ( isVideoTrack( handle, trackId ) )
         trackIds.push_back( trackId );
   }
   return trackIds;
}

int main()
{
   std::string path = "C:\\Users\\d.cheng.TSCCORP\\Desktop\\bugs\\2856 - Crash importing fuzzed MP4\\fuzzed.mp4";
   MP4FileHandle fh = MP4Read( path.c_str(), parseAtomCallback );

   std::vector<MP4TrackId> videoTrackIds = mp4TrackIdsOfAllVideoTracks( fh );
   for ( MP4TrackId videoTrackId : videoTrackIds )
   {
      const char * pFourccStr = ::MP4GetTrackMediaDataName( fh, videoTrackId );
      if ( pFourccStr == nullptr || strlen( pFourccStr ) != 4 )
         return false;  // invalid fourCC, track can't be supported
   }
   return true;

   MP4Close( fh );
}

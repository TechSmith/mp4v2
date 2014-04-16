./configure --disable-fvisibility CXXFLAGS="-stdlib=libstdc++"
make -j8
cp .libs/*.a lib/mac/Release

build : 
	mkdir -p release
	mkdir -p debug
	cd debug; cmake -DCMAKE_BUILD_TYPE=Debug ..
	cd release; cmake -DCMAKE_BUILD_TYPE=Release ..


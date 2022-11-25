thread_pool_test:
	# -l 去掉lib和.so
	g++ -o example example.cpp -std=c++17 -lpool -lpthread

clean:
	rm -f thread_pool_test


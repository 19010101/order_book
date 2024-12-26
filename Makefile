.PHONY: default experiment3 experiment3_rel release/tests_rng debug/tests_rng

default :experiment3_rel

experiment3: debug/tests_rng
	$^ experiment3
experiment3_rel: release/tests_rng
	$^ experiment3
release/tests_rng: 
	cmake --build release --target tests_rng
debug/tests_rng:
	cmake --build debug --target tests_rng

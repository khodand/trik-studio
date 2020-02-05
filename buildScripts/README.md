CONFIG+=tests CONFIG+=noPch CONFIG+=ccache CONFIG+=sanitize_undefined CONFIG+=sanitize_address

ASAN_OPTIONS=detect_leaks=0:abort_on_error=1:use_sigaltstack=0:detect_stack_use_after_return=1


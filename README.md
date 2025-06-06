# Parallel-Scanning-Test
Parallel Scanning Test using std::thread
# Ideas
1. json text length >> json token (candidate) array length
2. " here " (state 1) or "   "  here  (state 0) or here  "    " (state 0)
3. run two cases : 1) start_state == 0, 2) start_state == 1
4. first_state is zero
5. construct last_state[thr_num][2] // used thread num
6. next_start_state = last_state[t][start_state];
   

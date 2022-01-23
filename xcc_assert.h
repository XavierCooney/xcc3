NORETURN void xcc_assert_error_internal(const char *msg, int compiler_line_num,
                               const char* compiler_file_name,
                               const char *compiler_func_name);

#define xcc_assert_msg(condition, msg)  do { \
                                            /* infinite loop to act as noreturn */ \
                                            while(!(condition)) \
                                            xcc_assert_error_internal( \
                                                (msg), __LINE__, \
                                                __FILE__, __func__ \
                                            ); \
                                        } while(false);

#define xcc_assert(condition) xcc_assert_msg((condition), NULL)

#define xcc_assert_not_reached_msg(msg) xcc_assert_msg(false, (msg))

#define xcc_assert_not_reached() xcc_assert_not_reached_msg(NULL)

#!/usr/bin/python3.8

import os, subprocess, sys, re

TEST_DIRECTORY = 'tests/'
NO_MAKE = '--no-make' in sys.argv
ASSEMBLY_OUTPUT_FILE = 'build/out.S'
BINARY_OUTPUT_LOCATION = 'build/out'

print(' === Beginning main test suite == ')

if not NO_MAKE:
    subprocess.run(['make', 'all'], check=True)
    print('Compiled xcc')

SUCCESS = 'success'
FAILURE = 'failure'

def run_test(test_file_name):
    if 'notest' in test_file_name: return (SUCCESS,)

    test_file_path = os.path.join(TEST_DIRECTORY, test_file_name)

    with open(test_file_path) as test_file:
        source = test_file.read()

    checked_flags_params = set()

    def has_any_flag(*flags):
        checked_flags_params.update(f'@{flag}!' for flag in flags)
        return any(f'@{flag}!' in source for flag in flags)

    def get_param_values(param):
        checked_flags_params.add(f'@{param}:')
        param_values = []
        param = f'@{param}: '
        for line in source.split('\n'):
            if param in line:
                param_values.append(line.split(param, 1)[1])
        return param_values

    xcc_captured_output = subprocess.run(
        ['./xcc', test_file_path, '-o', ASSEMBLY_OUTPUT_FILE]
        + (['-v'] if has_any_flag('compile_verbose') else []),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE
    )

    if xcc_captured_output.stdout:
        return (FAILURE, 'gave stdout', xcc_captured_output)

    decoded_stderr = xcc_captured_output.stderr.decode('utf-8')
    for compile_error_line in get_param_values('xcc_msg'):
        if compile_error_line not in decoded_stderr:
            return (FAILURE, f"compile error doesn't have: `{compile_error_line}`", xcc_captured_output)

    xcc_should_have_been_success = has_any_flag('run', 'compiles')
    xcc_should_have_been_failure = has_any_flag('compile_error')
    if not (xcc_should_have_been_success or xcc_should_have_been_failure):
        return (FAILURE, 'neither failure nor success for xcc specified', xcc_captured_output)

    if xcc_should_have_been_failure:
        if xcc_captured_output.returncode == 0:
            return (FAILURE, "xcc should've failed but didn't", xcc_captured_output)

        if not xcc_captured_output.stderr:
            return (FAILURE, 'no stderr given on error', xcc_captured_output)

    if xcc_should_have_been_success:
        if xcc_captured_output.returncode != 0:
            return (FAILURE, "compile error", xcc_captured_output)

        if not has_any_flag('compile_verbose') and xcc_captured_output.stderr:
            return (FAILURE, "output to stderr but no compile error")

    # TODO: manually assemble and link
    do_link = has_any_flag('links', 'run')
    if do_link:
        subprocess.run([
            'gcc', '-o', BINARY_OUTPUT_LOCATION,
            ASSEMBLY_OUTPUT_FILE, 'supplement.c'
        ], check=True)

    do_run = has_any_flag('run')
    if do_run:
        captured_output = subprocess.run(
            [BINARY_OUTPUT_LOCATION],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        for expected_return_code in get_param_values('rc'):
            if str(captured_output.returncode) != expected_return_code:
                return (
                    FAILURE,
                    f'unexpected return code ({captured_output.returncode}) on execution'
                    f' (epected {expected_return_code})',
                )

    # check that all flags provided were actually checked (prevent mispellings)
    found_flags = re.findall(r'@[a-z_]+[!:]', source)
    for flag_in_src in found_flags:
        if flag_in_src not in checked_flags_params:
            return (FAILURE, f'unchecked param: {flag_in_src}')

    # this'll have some false positives
    num_ats = source.count('@')
    if num_ats > len(found_flags):
        return (FAILURE, 'possible malformed test flags/params')
    return (SUCCESS,)


all_results = []
had_failure = False
print(' ', end='')
for test_file_name in os.listdir(TEST_DIRECTORY):
    res = run_test(test_file_name)
    all_results.append((test_file_name, res))
    if res[0] != SUCCESS:
        print()
        # give the failure early
        print(' == Failure in', test_file_name, ' ===')
        print('\t', res[1])
        for extra_info in res[2:]:
            if type(extra_info) == subprocess.CompletedProcess:
                print('\t', 'stdout:', extra_info.stdout)
                if extra_info.stderr:
                    print('\t', 'stderr:')
                    decoded = extra_info.stderr.decode('utf-8')
                    decoded = decoded.strip() # maybe don't do this?
                    for line in decoded.split('\n'):
                        print('\t\t', line)
                else:
                    print('\t', 'no stderr')
        had_failure = True
    elif not had_failure:
        sys.stdout.write('✓')
        sys.stdout.flush() # print should flush right?

if had_failure:
    print(' ================================ ')
else:
    print()

successes = sum(1 for result in all_results if result[1][0] == SUCCESS)
print('  ', successes, 'successes:', '.' * successes)
if had_failure:
    num_failures = len(all_results) - successes
    print('  ', num_failures, 'failures:', '!' * num_failures)

print(' ==== End of main test suite ==== ')

if had_failure:
    sys.exit(1)
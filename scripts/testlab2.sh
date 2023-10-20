WORKDIR=$(dirname "$(dirname "$(readlink -f "$0")")")

ref_dir=${WORKDIR}/testdata/lab3/refs
testcase_dir=${WORKDIR}/testdata/lab3/testcases
# ref=${ref_dir}/$1".out"
# testcase=${testcase_dir}/$1".tig"
#${WORKDIR}/build/test_parse ${testcase} >/tmp/output.txt
#diff -c /tmp/output.txt "${ref}"
#if [[ $? != 0 ]]; then
#    echo "Error: Output mismatch"
#    echo "0"
#    exit 1
#else
#    echo "100"
#    exit 0
#fi
for testcase in "$testcase_dir"/*.tig; do
    testcase_name=$(basename "$testcase" | cut -f1 -d".")
    ref=${ref_dir}/${testcase_name}.out

    ${WORKDIR}/build/test_parse "$testcase" >&/tmp/output.txt
    diff -c /tmp/output.txt "${ref}"
    if [[ $? != 0 ]]; then
            echo "Error: Output mismatch"
            echo $testcase_name
            echo "${score_str}: 0"
            exit 1
    else
            echo $testcase_name" pass"
    fi
done

echo "[^_^]: Pass"
echo "${score_str}: 100"
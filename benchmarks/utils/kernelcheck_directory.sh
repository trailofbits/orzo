kernelcheck_binary_path=$1
linux_results_dir=$2
warnings_file=$3

temp_file=$(mktemp /tmp/kernelcheck-results.XXXXX)

# The script to run the benchmark appends an underscore to the file extension to
# prevent duplicates, so we need to do mlir* to match on those files as well.
for file in `ls "$linux_results_dir"/*.mlir*`
do
    # Dump stdout to /dev/null since we're only interested in the warnings.
    /usr/bin/time -f%E "$kernelcheck_binary_path" --kernelcheck $file  \
        2>> "$temp_file" 1>/dev/null
done

grep -P "(info:)|(warning:)" "$temp_file" | sort -u > $warnings_file

rm "$temp_file"

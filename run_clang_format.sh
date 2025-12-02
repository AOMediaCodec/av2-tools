find src include apps tests examples \
    -type f \( -name "*.cpp" -o -name "*.h" \) \
    -exec clang-format -i {} +

echo "All files formatted!"
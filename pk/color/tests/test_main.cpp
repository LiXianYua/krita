int run_color_tests();

int main(int argc, char** argv)
{
    int failures = run_color_tests();
    if (failures == 0) {
        return 0;
    }
    return 1;
}

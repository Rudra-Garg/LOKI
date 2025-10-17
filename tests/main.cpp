#include <gtest/gtest.h>
#include <QApplication> // Required for Qt components if they are used indirectly

int main(int argc, char **argv) {
    // Some Qt components might need a QApplication instance, even in tests.
    // We create a non-GUI one here to be safe.
    QCoreApplication app(argc, argv);

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
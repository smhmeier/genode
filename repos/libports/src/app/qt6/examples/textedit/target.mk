ifeq ($(CONTRIB_DIR),)
QT6_BASE_DIR       = $(call select_from_repositories,src/lib/qt6_base)
else
QT6_BASE_PORT_DIR := $(call select_from_ports,qt6_base)
QT6_BASE_DIR       = $(QT6_BASE_PORT_DIR)/src/lib/qt6_base
endif

QMAKE_PROJECT_FILE = $(QT6_BASE_DIR)/tests/manual/examples/widgets/richtext/textedit/textedit.pro

QMAKE_TARGET_BINARIES = textedit

QT6_PORT_LIBS = libQt6Core libQt6Gui libQt6PrintSupport libQt6Widgets

LIBS = qt6_qmake libc libm mesa qt6_component stdcxx

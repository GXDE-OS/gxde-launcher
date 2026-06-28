#!/bin/bash
cd `dirname $0`
/usr/lib/qt6/bin/lupdate -recursive src/ -ts translations/gxde-launcher_*.ts

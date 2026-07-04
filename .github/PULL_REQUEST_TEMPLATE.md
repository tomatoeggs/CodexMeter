# PR 说明

## 改动内容

- 

## 验证

- [ ] `.venv/bin/python tests/run_tests.py`
- [ ] `PLATFORMIO_CORE_DIR="$PWD/.platformio" UV_CACHE_DIR="$PWD/.platformio/.cache/uv" .venv/bin/pio run -d firmware -e waveshare_amoled_216`
- [ ] 已在设备上验证
- [ ] 仅文档修改，无需运行测试

## 注意事项

请确认没有提交 token、cookie、个人日志、`.venv/`、`.platformio/`、`.npm-cache/` 或构建产物。

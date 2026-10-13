# moNa2 layer notifier (src/layer_notifier.c): 有効レイヤーを Raw HID でホストへ送る
if(CONFIG_MONA2_LAYER_NOTIFIER)
  if(NOT CONFIG_RAW_HID)
    message(FATAL_ERROR "MONA2_LAYER_NOTIFIER requires CONFIG_RAW_HID=y (zmk-raw-hid と raw_hid_adapter shield を追加してください)")
  endif()
  target_sources(app PRIVATE src/layer_notifier.c)
endif()

# 第三方组件与模型说明

## sherpa-onnx 1.13.8

- 来源：https://github.com/k2-fsa/sherpa-onnx/releases/tag/v1.13.8
- 文件：`app/libs/sherpa-onnx-1.13.8.aar`
- 许可证：Apache License 2.0
- SHA-256：`633C24321E06B1FE79FEAFA03EA16CBC0F8A286641E2DA3559BAC91BDB13BD96`

## Streaming Paraformer bilingual zh-en INT8

- 来源：https://huggingface.co/csukuangfj/sherpa-onnx-streaming-paraformer-bilingual-zh-en
- 上游模型：ModelScope `damo/speech_paraformer_asr_nat-zh-cn-16k-common-vocab8404-online`
- 许可证：Apache License 2.0
- `encoder.int8.onnx` SHA-256：`81A70226A8934E6ED92AA1D4FC486B428B5398E2F2619ED4897B7294CAB90E9A`
- `decoder.int8.onnx` SHA-256：`F3CCA9F77BB9D93C8FCBFB63AE617B6B1EE96818DF3AA3B151C40658FE38594F`

## 3D-Speaker CAM++ zh-cn 16k

- 来源：https://github.com/k2-fsa/sherpa-onnx/releases/tag/speaker-recongition-models
- 上游项目：Alibaba DAMO Academy 3D-Speaker
- 文件：`app/src/main/assets/3dspeaker_speech_campplus_sv_zh-cn_16k-common.onnx`
- 许可证：Apache License 2.0
- SHA-256：`F682B514C05D947EE3FA91CD6EC6C5C7543479A128373FA29B1FAEDCCD21FD11`

以上组件仅用于本地离线语音与声纹识别。录音不会发送给模型来源方或其他在线服务。

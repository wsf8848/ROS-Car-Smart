# Bee ROS Web Console 地图存储规范

## 固定位置

所有正式建图结果只保存到 RDK X5 板端：

```text
/opt/bee-ros-web-console/data/maps/
```

该目录与网页源码、ROS 工作空间分离，升级网页代码不会覆盖地图。

## 文件格式

每张地图必须成对存在：

```text
<map_id>.yaml   # 地图元数据：分辨率、原点、占据阈值和图像文件名
<map_id>.pgm    # 占据栅格图像
<map_id>.json   # Bee元数据：自定义地图名、场地、创建时间
```

网页“保存当前地图”调用 ROS `nav2_map_server map_saver_cli` 写入该目录；网页“加载”只接受目录内已存在且成对完整的地图 ID。

## 操作约束

- 新建地图完成后必须人工检查栅格、原点、分辨率和障碍物，再作为导航地图使用。
- 需要导航或巡逻时，在网页地图管理中选择“加载”；加载完成并定位成功后才允许开始任务。
- 不把实时 `/scan`、临时 `/map` 或代价地图直接当作永久地图。
- 不手工修改 YAML 中的 `image`、`resolution`、`origin` 等关键字段；需要变更时重新保存新地图版本。
- 地图备份由项目备份脚本复制 `data/maps`，恢复时保持 `.yaml/.pgm` 同名配对。
- 自定义地图名称、场地和创建时间保存在同名 `.json` 侧车文件中，不改动 ROS 标准 YAML/PGM；旧地图缺少 JSON 时自动回退为地图 ID 和文件时间。

## 当前服务配置

systemd `bee-web-console.service` 使用：

```text
BEE_MAPS_ROOT=/opt/bee-ros-web-console/data/maps
```

该路径是板端唯一地图库。前端和网关只通过 `/api/maps`、`/api/maps/action` 访问，不接受浏览器提交任意文件路径或 shell 命令。

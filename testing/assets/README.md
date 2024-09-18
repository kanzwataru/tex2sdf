# Test Assets
These test assets are from Chris Cummings' [article series about SDFs](https://shaderfun.com/2018/03/23/signed-distance-fields-part-1-unsigned-distance-fields/).

## SDF Conversion

Here are the settings used for converting the SDFs (for later tests).

```
build/tex2sdf test_assets/rectangles.png test_assets/rectangles_sdf.tga --sdf_range 16
build/tex2sdf test_assets/catlores.jpg test_assets/catlores_sdf.tga --sdf_range 8
build/tex2sdf test_assets/aliaslines.png test_assets/aliaslines_sdf.tga --sdf_range 32
build/tex2sdf test_assets/cathires.jpg test_assets/cathires.tga --sdf_range 100
```

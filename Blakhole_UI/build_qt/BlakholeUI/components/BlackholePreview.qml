// BlackholePreview.qml — Canvas 黑洞预览组件
import QtQuick
import QtQuick.Controls

Item {
    id: root

    // === 黑洞参数 ===
    property real diskTemp: 5500       // 色温 K
    property real diskIncl: 1.5        // 盘面倾角 (0=正面, PI/2=侧面)
    property real diskRoll: 0.35       // 盘面旋转
    property real diskInner: 1.8       // 内半径
    property real diskOuter: 8.0       // 外半径
    property real diskOpac: 0.9        // 不透明度
    property real diskDopp: 0.6        // 多普勒效应
    property real diskBeam: 2.5        // 光束指数
    property real diskGain: 2.2        // 亮度增益
    property real diskContr: 1.6       // 对比度
    property real diskWind: 7.0        // 缠绕紧度
    property real diskSpeed: 5.0       // 旋转速度
    property real diskExpo: 1.4        // 曝光度
    property real diskStar: 0.0        // 星空亮度

    // === 内部状态 ===
    property real rotationAngle: 0
    property int frameCount: 0

    width: 400
    height: 400

    Canvas {
        id: canvas
        anchors.fill: parent
        anchors.margins: 8
        renderStrategy: Canvas.CooperativeRenderStrategy

        onPaint: {
            var ctx = getContext("2d")
            var w = width
            var h = height
            var cx = w / 2
            var cy = h / 2
            var maxR = Math.min(w, h) / 2 - 4

            ctx.clearRect(0, 0, w, h)

            // === 1. 星空背景 ===
            var bgGrad = ctx.createRadialGradient(cx, cy, 0, cx, cy, maxR)
            bgGrad.addColorStop(0, "#0a0a0f")
            bgGrad.addColorStop(1, "#020205")
            ctx.fillStyle = bgGrad
            ctx.fillRect(0, 0, w, h)

            // === 2. 星空 ===
            if (diskStar > 0.01) {
                // 用伪随机固定星空位置
                var starSeed = 42
                function pseudoRandom(s) {
                    var x = Math.sin(s * 127.1 + 311.7) * 43758.5453
                    return x - Math.floor(x)
                }
                for (var i = 0; i < Math.floor(diskStar * 200); i++) {
                    var sx = pseudoRandom(starSeed + i * 3) * w
                    var sy = pseudoRandom(starSeed + i * 3 + 1) * h
                    var sr = pseudoRandom(starSeed + i * 3 + 2) * 1.5 + 0.3
                    var sa = pseudoRandom(starSeed + i * 3 + 3) * 0.5 + 0.3
                    ctx.fillStyle = "rgba(255,255,255," + sa + ")"
                    ctx.beginPath()
                    ctx.arc(sx, sy, sr, 0, Math.PI * 2)
                    ctx.fill()
                }
            }

            ctx.save()
            ctx.translate(cx, cy)
            ctx.rotate(diskRoll)

            // === 3. 引力透镜光环 (最外层辉光) ===
            var lensR = maxR * 0.8
            var lensGrad = ctx.createRadialGradient(0, 0, maxR * 0.35, 0, 0, lensR)
            lensGrad.addColorStop(0, "rgba(255,180,80,0)")
            lensGrad.addColorStop(0.55, "rgba(255,180,80,0)")
            lensGrad.addColorStop(0.7, "rgba(255,140,40," + (diskGain * 0.15) + ")")
            lensGrad.addColorStop(0.82, "rgba(255,100,20," + (diskGain * 0.08) + ")")
            lensGrad.addColorStop(1, "rgba(255,60,10,0)")
            ctx.fillStyle = lensGrad
            ctx.beginPath()
            ctx.arc(0, 0, lensR, 0, Math.PI * 2)
            ctx.fill()

            // === 4. 光子环 (事件视界外缘) ===
            var photonR = maxR * 0.08
            var photonGrad = ctx.createRadialGradient(0, 0, photonR * 0.6, 0, 0, photonR * 1.4)
            photonGrad.addColorStop(0, "rgba(255,200,150," + (diskGain * 0.4) + ")")
            photonGrad.addColorStop(0.5, "rgba(255,150,80," + (diskGain * 0.3) + ")")
            photonGrad.addColorStop(1, "rgba(255,80,20,0)")
            ctx.fillStyle = photonGrad
            ctx.beginPath()
            ctx.arc(0, 0, photonR * 1.4, 0, Math.PI * 2)
            ctx.fill()

            // === 5. 吸积盘 (倾斜椭圆) ===
            var scaleY = Math.cos(diskIncl * 0.8)
            if (scaleY < 0.08) scaleY = 0.08

            var innerPx = maxR * diskInner / diskOuter * 0.7
            var outerPx = maxR * 0.7

            // 色温映射
            var tempNorm = (diskTemp - 1000) / 29000
            if (tempNorm < 0) tempNorm = 0
            if (tempNorm > 1) tempNorm = 1

            // 多层同心椭圆绘制吸积盘
            var layers = 40
            for (var j = 0; j < layers; j++) {
                var t = j / (layers - 1)  // 0=inner, 1=outer
                var r = innerPx + (outerPx - innerPx) * t
                var nextR = innerPx + (outerPx - innerPx) * ((j + 1) / (layers - 1))
                var bandWidth = nextR - r

                // 温度从内到外递减 (内圈热=蓝白, 外圈冷=红)
                var localTemp = 1 - t
                var r_col, g_col, b_col

                if (localTemp > 0.6) {
                    // 蓝白 (高温内圈)
                    var bt = (localTemp - 0.6) / 0.4
                    r_col = Math.floor(180 + 75 * bt)
                    g_col = Math.floor(180 + 75 * bt)
                    b_col = Math.floor(200 + 55 * bt)
                } else if (localTemp > 0.3) {
                    // 白到橙
                    var wt = (localTemp - 0.3) / 0.3
                    r_col = Math.floor(255)
                    g_col = Math.floor(180 + 75 * wt)
                    b_col = Math.floor(100 + 100 * (1 - wt))
                } else {
                    // 橙到红 (低温外圈)
                    var rt = localTemp / 0.3
                    r_col = Math.floor(200 + 55 * rt)
                    g_col = Math.floor(80 + 60 * rt)
                    b_col = Math.floor(30 + 20 * rt)
                }

                // 多普勒效应: 左侧更亮
                var doppFactor = 1.0
                // 在椭圆上采样应用
                var alpha = diskOpac * diskExpo * 0.7

                ctx.fillStyle = "rgba(" + r_col + "," + g_col + "," + b_col + "," + alpha + ")"
                ctx.beginPath()
                ctx.ellipse(0, 0, nextR, nextR * scaleY)
                ctx.ellipse(0, 0, r, r * scaleY)
                ctx.fill("evenodd")
            }

            // === 6. 多普勒亮度不对称 ===
            if (diskDopp > 0.01) {
                var doppAlpha = diskDopp * diskGain * 0.15
                var doppGrad = ctx.createLinearGradient(-outerPx, 0, outerPx, 0)
                doppGrad.addColorStop(0, "rgba(255,255,255," + doppAlpha + ")")
                doppGrad.addColorStop(0.3, "rgba(255,255,255," + (doppAlpha * 0.5) + ")")
                doppGrad.addColorStop(0.5, "rgba(255,255,255,0)")
                doppGrad.addColorStop(1, "rgba(255,255,255,0)")
                ctx.fillStyle = doppGrad
                ctx.beginPath()
                ctx.ellipse(0, 0, outerPx, outerPx * scaleY)
                ctx.ellipse(0, 0, innerPx, innerPx * scaleY)
                ctx.fill("evenodd")
            }

            // === 7. 事件视界 (纯黑圆) ===
            var horizonR = maxR * 0.04
            ctx.fillStyle = "#000000"
            ctx.beginPath()
            ctx.arc(0, 0, horizonR, 0, Math.PI * 2)
            ctx.fill()

            // 事件视界边缘微光环
            var edgeGrad = ctx.createRadialGradient(0, 0, horizonR * 0.7, 0, 0, horizonR * 1.15)
            edgeGrad.addColorStop(0, "rgba(60,60,80,0.8)")
            edgeGrad.addColorStop(1, "rgba(60,60,80,0)")
            ctx.fillStyle = edgeGrad
            ctx.beginPath()
            ctx.arc(0, 0, horizonR * 1.15, 0, Math.PI * 2)
            ctx.fill()

            ctx.restore()
        }
    }

    // === 动画旋转 ===
    Timer {
        interval: 50
        running: true
        repeat: true
        onTriggered: {
            rotationAngle += diskSpeed * 0.02
            if (rotationAngle > Math.PI * 2) rotationAngle -= Math.PI * 2
            frameCount++
            // 每2帧刷新一次 Canvas
            if (frameCount % 2 === 0) {
                canvas.requestPaint()
            }
        }
    }

    // 参数变化时立即重绘
    onDiskTempChanged: canvas.requestPaint()
    onDiskInclChanged: canvas.requestPaint()
    onDiskRollChanged: canvas.requestPaint()
    onDiskInnerChanged: canvas.requestPaint()
    onDiskOuterChanged: canvas.requestPaint()
    onDiskOpacChanged: canvas.requestPaint()
    onDiskDoppChanged: canvas.requestPaint()
    onDiskBeamChanged: canvas.requestPaint()
    onDiskGainChanged: canvas.requestPaint()
    onDiskContrChanged: canvas.requestPaint()
    onDiskWindChanged: canvas.requestPaint()
    onDiskSpeedChanged: canvas.requestPaint()
    onDiskExpoChanged: canvas.requestPaint()
    onDiskStarChanged: canvas.requestPaint()
}

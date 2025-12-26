#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <vector>

#include <QPixmap>
#include <QImage>
// ==========================================
// 1. 光学计算逻辑与结构定义
// ==========================================
struct LensResult
{
    double f_prime;          // 焦距
    double l_H;              // 前主面位置 (相对于第一面顶点)
    double l_prime_H_prime;  // 后主面位置 (相对于第二面顶点)
    double L_prime;          // 像距 (相对于第二面顶点)
    double beta;             // 横向放大率
};

class math_algorithm
{
public:
    // Zemax 折射率公式计算 (题目图片给出的具体公式)
    static double calculateN(double nd, double vd, double lamda)
    {
        const double C1 = -0.335562, C2 = 0.41207, C3 = -0.127723;
        const double D1 = -0.0141585, D2 = 0.010522, D3 = 0.115844;
        const double E1 = -0.000113289, E2 = -0.000028339, E3 = 0.734981;
        const double ld = 0.58756; // d光波长参考值 (um)

        double L1 = lamda * lamda - ld * ld;
        double L2 = 1.0 / (lamda * lamda) - 1.0 / (ld * ld);
        double L3 = 1.0 / std::pow(lamda, 4) - 1.0 / std::pow(ld, 4);

        return nd + (C1 + C2 * nd + C3 * nd * nd) * L1
                  + (D1 + D2 * nd + D3 / vd) * L2
                  + (E1 + E2 * nd + E3 / (vd * vd)) * L3;
    }

    // 厚透镜高斯参数计算
    static LensResult calculate(double R1, double R2, double d, double n, double L_obj)
    {
        LensResult res;
        // 光焦度 Phi = (n-1)*[1/R1 - 1/R2 + (n-1)*d/(n*R1*R2)]
        double phi = (n - 1.0) * (1.0/R1 - 1.0/R2 ) + (n - 1.0)*(n - 1.0)*d/(n*R1*R2);
        //double phi = (n - 1.0) * (1.0 / R1 - 1.0 / R2 + (n - 1.0) * d / (n * R1 * R2));
        res.f_prime = 1.0 / phi;

        // 主面位置公式
        res.l_H = -(R1 * d) / (n*(R2-R1)+(n-1)*d);
        //res.l_H = -(res.f_prime * (n - 1.0) * d) / (n * R2);
        res.l_prime_H_prime = -(R2*d) / (n * (R2-R1) + (n-1) * d);
        //res.l_prime_H_prime = -(res.f_prime * (n - 1.0) * d) / (n * R1);


        // 高斯公式: 物距 s 相对于前主面
        double s = L_obj - res.l_H;
        if (std::abs(1.0/s + phi) < 1e-9)
        {
            res.L_prime = 1e9; // 象征无穷远
            res.beta = 0;
        } else
        {
            double s_prime = 1.0 / (1.0 / s + phi);
            res.L_prime = s_prime + res.l_prime_H_prime; // 像距相对于第二面顶点
            res.beta = s_prime / s;
        }
        return res;
    }
};

// ==========================================
// 2. 自定义绘图控件 (透镜形状 + 色差曲线)
// ==========================================
class LensCanvas : public QWidget
{
public:
    double R1 = 50, R2 = -50, d = 10, nd = 1.5168, vd = 64.17, L_obj = -100;
    bool show_switch = false;

    LensCanvas(QWidget *parent = nullptr) : QWidget(parent)
    {
        setBackgroundRole(QPalette::Base);   //设置背景颜色角色
        setAutoFillBackground(true);         //每次重绘前自动填充背景色，防止绘图出现重影。
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);   //开启抗锯齿
        //QPixmap pixmap("../Image/xiaohui.png");
        if (show_switch)                 //如果为True,则调用drawDispersionCurve 函数画曲线
        {
            drawDispersionCurve(painter);
        } else
        {
            drawLensShape(painter);     //否则，调用 drawLensShape 函数画透镜的物理形状图
        }
    }

private:
    // 绘制透镜结构图
    void drawLensShape(QPainter &p)
    {
        //放置校徽
        QPixmap pix("../Image/xiaohui.png");
        if (!pix.isNull())
        {
                // 自动缩放图片（例如宽度设为200，高度按比例缩放）
                QPixmap scaledPix = pix.scaledToWidth(200, Qt::SmoothTransformation);

                // 计算右下角的坐标 (留出20像素的边距)
                int x = width() - scaledPix.width() - 20;
                int y = height() - scaledPix.height() - 20;

                // 绘制图片
                p.drawPixmap(x, y, scaledPix);
         }

        int w = width(), h = height();
        p.translate(w / 4, h / 2);
        double scale = 2.0;     //缩放比例

        LensResult res = math_algorithm::calculate(R1, R2, d, nd, L_obj);

        // 绘制光轴
        p.setPen(QPen(Qt::lightGray, 1, Qt::DashLine));
        p.drawLine(-100, 0, 400, 0);

        // 绘制透镜示意形状
        p.setPen(QPen(Qt::blue, 2));
        p.setBrush(QColor(100, 150, 255, 60));
        QPainterPath path;
        double lensH = 40; // 透镜半高度
        path.moveTo(0, -lensH);
        // 简单模拟圆弧：这里用二次贝塞尔曲线示意凹凸
        path.quadTo(R1 > 0 ? 10 : -10, 0, 0, lensH);
        path.lineTo(d * scale, lensH);
        path.quadTo(R2 > 0 ? d*scale+10 : d*scale-10, 0, d*scale, -lensH);
        path.closeSubpath();
        p.drawPath(path);

        // 标注主面 H, H' (红色线)
        p.setPen(QPen(Qt::red, 1));
        double h_pos = res.l_H * scale;
        double hp_pos = (d + res.l_prime_H_prime) * scale;
        p.drawLine(h_pos, -50, h_pos, 50); p.drawText(h_pos, 65, "H");
        p.drawLine(hp_pos, -50, hp_pos, 50); p.drawText(hp_pos, 65, "H'");

        // 标注焦点 F'
        double f_pos = (d + res.l_prime_H_prime + res.f_prime) * scale;
        p.setBrush(Qt::red);
        p.drawEllipse(QPointF(f_pos, 0), 3, 3);
        p.drawText(f_pos, -10, "F'");
    }

    // 绘制色差曲线 (模拟题目给出的纵轴波长、横轴焦距偏移)
    void drawDispersionCurve(QPainter &p)
    {
        int w = width();
        int h = height();

        // 1. 定义图形区域的边距
        int marginLeft = 80;    // 左侧留出空间写波长数值
        int marginBottom = 60;  // 底部留出空间写偏移数值
        int marginTop = 40;
        int marginRight = 40;

        int graphW = w - marginLeft - marginRight;
        int graphH = h - marginTop - marginBottom;

        // 2. 设置坐标范围
        double lamMin = 0.486;    // F线波长
        double lamMax = 0.6563;   // C线波长
        double shiftRange = 1000.0; // 横轴范围设为 +/- 1000 um (即 10e2)

        // 绘制坐标轴线
        p.setPen(Qt::black);
        // 横轴 (底部)
        p.drawLine(marginLeft, h - marginBottom, w - marginRight, h - marginBottom);
        // 纵轴 (左侧)
        p.drawLine(marginLeft, h - marginBottom, marginLeft, marginTop);
        // 零位参考线 (中央垂直黑线)
        int centerX = marginLeft + graphW / 2;
        p.drawLine(centerX, h - marginBottom, centerX, marginTop);

        // --- 3. 绘制 Y 轴刻度与数值 (波长 Wavelength) ---
        p.setFont(QFont("Arial", 8));
        double yTicks[] = {0.486, 0.52, 0.56, 0.60, 0.64, 0.6563};
        for (double lam : yTicks) {
            // 计算像素位置：底部是最小波长，顶部是最大波长
            int y = h - marginBottom - (int)((lam - lamMin) / (lamMax - lamMin) * graphH);

            // 画刻度线
            p.drawLine(marginLeft, y, marginLeft - 5, y);
            // 画数值
            p.drawText(marginLeft - 50, y + 5, QString::number(lam, 'f', 4));
        }
        // Y轴标题
        p.save();
        p.translate(marginLeft - 60, h / 2);
        p.rotate(-90);
        p.drawText(0, 0, "Wavelength in um");
        p.restore();

        // --- 4. 绘制 X 轴刻度与数值 (焦距偏移 Focal Shift) ---
        // 从 -1000 到 1000 每 200 一个刻度
        for (int s = -1000; s <= 1000; s += 200) {
            int x = centerX + (int)((s / shiftRange) * (graphW / 2.0));

            // 只画区域内的刻度
            if (x >= marginLeft && x <= w - marginRight) {
                p.drawLine(x, h - marginBottom, x, h - marginBottom + 5);

                // 格式化文本，模仿图片中的 -10e2, -800.0 等
                QString label;
                if (abs(s) == 1000) label = QString::number(s/100) + "e2";
                else label = QString::number((double)s, 'f', 1);

                p.drawText(x - 20, h - marginBottom + 20, label);
            }
        }
        // X轴标题
        p.drawText(centerX - 50, h - 10, "Focal Shift in um");

        // --- 5. 绘制色差曲线 (核心曲线) ---
        p.setPen(QPen(Qt::blue, 1.5));
        double f_ref = math_algorithm::calculate(R1, R2, d, nd, L_obj).f_prime;

        QPointF lastPt;
        bool first = true;
        for (int i = 0; i <= 100; ++i) {
            double lam = lamMin + i * (lamMax - lamMin) / 100.0;
            double n_lam = math_algorithm::calculateN(nd, vd, lam);
            double f_lam = math_algorithm::calculate(R1, R2, d, n_lam, L_obj).f_prime;

            // 计算物理偏移量 (转换为微米 um)
            double shift_um = (f_lam - f_ref) * 1000.0;

            // 转换为像素坐标
            int px = centerX + (int)((shift_um / shiftRange) * (graphW / 2.0));
            int py = h - marginBottom - (int)((lam - lamMin) / (lamMax - lamMin) * graphH);

            // 只绘制在图形区域内的点
            if (px >= marginLeft && px <= w - marginRight) {
                if (!first) p.drawLine(lastPt, QPointF(px, py));
                lastPt = QPointF(px, py);
                first = false;
            }
        }
        /*
        int w = width(), h = height();
        int margin = 60;
        p.translate(w/2, h - margin); // 中心参考点

        // 画坐标轴
        p.setPen(Qt::black);
        p.drawLine(-w/2 + 20, 0, w/2 - 20, 0); // 横轴 (Focal Shift)
        p.drawLine(0, 0, 0, -h + 100);          // 纵轴 (Wavelength)
        p.drawText(w/2 - 100, 20, "Focal Shift (um)");
        p.drawText(10, -h + 110, "Wavelength (um)");

        // 计算参考焦距 (d光)
        double f_ref = math_algorithm::calculate(R1, R2, d, nd, L_obj).f_prime;

        p.setPen(QPen(Qt::blue, 2));
        QPointF lastPt;
        bool first = true;

        // 遍历波长范围 0.486 (F) 到 0.656 (C)
        for (int i = 0; i <= 100; ++i)
        {
            double lam = 0.486 + i * (0.6563 - 0.486) / 100.0;
            double n_lam = math_algorithm::calculateN(nd, vd, lam);
            double f_lam = math_algorithm::calculate(R1, R2, d, n_lam, L_obj).f_prime;

            // 映射坐标
            double x = (f_lam - f_ref) * 1000; // 焦距变化量放大1000倍显示在轴上
            double y = -(lam - 0.486) * 1500; // 波长从底部向上延伸

            if (!first) p.drawLine(lastPt, QPointF(x, y));
            lastPt = QPointF(x, y);
            first = false;

         }
        */
    }
};

// ==========================================
// 3. 主窗口布局与交互
// ==========================================
class MainWindow : public QMainWindow
{
public:
    MainWindow()
    {
        QWidget *central = new QWidget;
        QHBoxLayout *mainLayout = new QHBoxLayout(central);

        // 左侧参数输入面板
        QVBoxLayout *panel = new QVBoxLayout;
        QFormLayout *form = new QFormLayout;

        edit_R1 = new QLineEdit("50");
        edit_R2 = new QLineEdit("-50");
        edit_D  = new QLineEdit("10");
        edit_Nd = new QLineEdit("1.5168");
        edit_Vd = new QLineEdit("64.17");
        edit_L  = new QLineEdit("-100");

        form->addRow("前曲率半径 R1 (mm):", edit_R1);
        form->addRow("后曲率半径 R2 (mm):", edit_R2);
        form->addRow("中心厚度 d (mm):", edit_D);
        form->addRow("折射率 nd:", edit_Nd);
        form->addRow("阿贝数 vd:", edit_Vd);
        form->addRow("物距 L (负值)(mm):", edit_L);

        QPushButton *btnCalc = new QPushButton("计算更新");
        QPushButton *btnMode = new QPushButton("切换 [形状图 / 色差曲线]");
        resLabel = new QLabel("计算结果将在此显示...");
        resLabel->setStyleSheet("color: darkgreen; font-weight: bold;");

        panel->addLayout(form);
        panel->addWidget(btnCalc);
        panel->addWidget(btnMode);
        panel->addWidget(resLabel);
        panel->addStretch();

        // 右侧绘图区域
        canvas = new LensCanvas;

        mainLayout->addLayout(panel, 1);
        mainLayout->addWidget(canvas, 3);
        setCentralWidget(central);
        resize(1100, 650);

        // 信号槽连接
        //QObject::connect(btnCalc, &QPushButton::clicked, [this]()
        connect(btnCalc, &QPushButton::clicked, [this]()
        {
            canvas->R1 = edit_R1->text().toDouble();
            canvas->R2 = edit_R2->text().toDouble();
            canvas->d  = edit_D->text().toDouble();
            canvas->nd = edit_Nd->text().toDouble();
            canvas->vd = edit_Vd->text().toDouble();
            canvas->L_obj = edit_L->text().toDouble();

            LensResult res = math_algorithm::calculate(canvas->R1, canvas->R2, canvas->d, canvas->nd, canvas->L_obj);
            resLabel->setText(QString("焦距 f': %1 mm\n前主面 lH: %2 mm\n后主面 l'H': %3 mm\n像距 L': %4 mm\n放大率 Beta: %5")
                              .arg(res.f_prime, 0, 'f', 4)
                              .arg(res.l_H, 0, 'f', 4)
                              .arg(res.l_prime_H_prime, 0, 'f', 4)
                              .arg(res.L_prime, 0, 'f', 4)
                              .arg(res.beta, 0, 'f', 4));
            canvas->update();
        });

        connect(btnMode, &QPushButton::clicked, [this]()
        {
            canvas->show_switch = !canvas->show_switch;
            canvas->update();
        });
        //美化部分
        this->setStyleSheet(R"(
                /* 窗口整体背景 */
                QMainWindow {
                    background-color: #f5f7fa;
                }

                /* 输入框美化 */
                QLineEdit
                {
                    border: 2px solid #dcdfe6;
                    border-radius: 6px;
                    padding: 5px 10px;
                    background: white;
                    selection-background-color: #409eff;
                    font-size: 14px;
                }
                QLineEdit:focus {
                    border: 2px solid #409eff; /* 获得焦点时变蓝 */
                }

                /* 按钮美化 */
                QPushButton {
                    background-color: #409eff;
                    color: white;
                    border-radius: 6px;
                    padding: 8px 15px;
                    font-weight: bold;
                    font-size: 14px;
                }
                QPushButton:hover {
                    background-color: #66b1ff; /* 鼠标悬停变浅 */
                }
                QPushButton:pressed {
                    background-color: #3a8ee6; /* 点击时变深 */
                }

                /* 切换按钮（特殊颜色） */
                QPushButton#btnMode {
                    background-color: #67c23a; /* 绿色 */
                }
                QPushButton#btnMode:hover {
                    background-color: #85ce61;
                }

                /* 标签美化 */
                QLabel {
                    font-family: "Microsoft YaHei";
                    font-size: 13px;
                    color: #606266;
                }

                /* 结果显示区域（通过 ObjectName 特殊处理） */
                QLabel#resLabel {
                    background-color: #ecf5ff;
                    border: 1px solid #d9ecff;
                    border-radius: 4px;
                    padding: 10px;
                    color: #409eff;
                    font-family: "Consolas", "Courier New"; /* 使用等宽字体显示数据 */
                    font-size: 14px;
                    line-height: 1.5;
                }
            )");
    }

private:
    QLineEdit *edit_R1, *edit_R2, *edit_D, *edit_Nd, *edit_Vd, *edit_L;
    QLabel *resLabel;
    LensCanvas *canvas;
};

// ==========================================
// 4. 程序入口
// ==========================================
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowTitle("单透镜复杂参数计算器");
    w.show();
    return a.exec();
}

from ROI import roi_begin as roi_begin
from ROI import roi_end as roi_end

x = [i for i in range(1, 50)]
y = [i for i in range(51, 100)]

roi_begin()
z = []
for i in range(0, len(x)):
    z.append(x[i] / y[i])
roi_end()

print(z)

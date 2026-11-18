M = imread('arnold_sample.png');
disp(size(M));
rows = size (M,1);
cols = size (M,2);
B = zeros (size(M) , 'like' , M);
for row = 1:rows
    for col = 1:cols
        x = row - 1;
        y = col - 1;
        x1 = mod(x + y , rows) + 1;
        y1 = mod(x + 2*y , rows) + 1;
        B (x1 , y1 , :) = M (row , col , :);
    end
end

R = zeros (size(M) , 'like' , M);
for row = 1:rows
    for col = 1:cols
        xp = row - 1;
        yp = col - 1;
        yp1 = mod(yp - xp , rows) + 1;
        xp1 = mod(2 * xp - yp , cols) + 1;
        R(xp1 , yp1 , :) = B(row , col , :);
    end
end

figure

subplot(1, 3, 1);
imshow(M);
title('原图');

subplot(1, 3 , 2);
imshow(B);
title('混淆图');

subplot(1 ,3 , 3);
imshow(R);
title('还原图');

disp(isequal(M,R));
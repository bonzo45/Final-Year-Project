figure(1); 

% Data
min = 0.25;
max = 0.75;
epsilon = 0.001;
x = [0, 1];
y = [0, 1];

% Font
set(gca, 'FontName', 'Quicksand');
set(gca, 'FontWeight', 'bold');

% Axes
% set(title('Variant 1: Opacity Transfer Function'), 'FontSize', 16);
set(xlabel('Mask Value'), 'FontWeight', 'bold');
set(ylabel('Opacity'), 'FontWeight', 'bold');
axis([0, 1, 0, 1.3]);
ax = gca;
ax.XTick = [0, 1];
ax.YTick = [0, 1];

hold on;

% Vertical Lines
% yL = get(gca,'YLim');
% line([min min], yL, 'Color', 'r', 'LineStyle', ':');
% line([max max], yL, 'Color', 'r', 'LineStyle', ':');

% Labels
% text([min], [1.1], ['<---'], 'VerticalAlignment','bottom', 'HorizontalAlignment','left');
% text([(min + max) / 2], [1.1], [' Threshold '], 'VerticalAlignment','bottom', 'HorizontalAlignment','center');
% text([max], [1.1], ['--->'], 'VerticalAlignment','bottom', 'HorizontalAlignment','right');

% Plot Data
plot(x, y, 'Color', 'b');
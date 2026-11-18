'use strict';

const get = (id) => document.getElementById(id);
const fields = { a: get('param-a'), b: get('param-b'), rounds: get('param-rounds') };
const form = get('transform-form');
const input = get('image-input');
const inputCanvas = get('input-canvas');
const outputCanvas = get('output-canvas');
const saveButton = get('save-button');
const transferButton = get('transfer-button');

const copy = {
  encrypt: {
    title: '图像加密', description: '用矩阵乘法重排像素，观察置乱前后的变化。',
    input: '原图', output: '置乱图', fileButton: '打开图片', inputEmpty: '选择方形图像',
    parameterNote: 'a = 1、b = 1 时与课件公式一致。',
    condition: '行列式为 1，矩阵在任意边长 N 下均可逆。',
    matrix: '加密矩阵 A', run: '运行加密', outputEmpty: '运行加密后查看结果'
  },
  decrypt: {
    title: '图像解密', description: '用逆矩阵逐轮撤销置乱，再检查图像是否恢复。',
    input: '置乱图', output: '恢复图', fileButton: '打开置乱图', inputEmpty: '导入置乱 PNG',
    parameterNote: '输入加密时使用的 a、b 和轮数 k。',
    condition: '保留原有边长和像素值，使用无损 PNG，避免压缩或缩放。',
    matrix: '解密矩阵 A⁻¹', run: '运行解密', outputEmpty: '运行解密后查看结果'
  }
};

function makeSession(message) {
  return { source: null, name: '', result: null, reference: null, loadId: 0,
    values: { a: '1', b: '1', rounds: '1' }, message, tone: '' };
}

const sessions = {
  encrypt: makeSession('选择图片和参数，然后运行加密。'),
  decrypt: makeSession('可打开已有的置乱 PNG，也可从加密页面将结果转入。')
};
let mode = 'encrypt';

function session() {
  return sessions[mode];
}

function keyFor(state) {
  const key = Object.fromEntries(Object.entries(state.values).map(([name, value]) => [name, Number(value)]));
  if (!Number.isInteger(key.a) || key.a < 1 || key.a > 99 ||
      !Number.isInteger(key.b) || key.b < 1 || key.b > 99 ||
      !Number.isInteger(key.rounds) || key.rounds < 1 || key.rounds > 9999) {
    return null;
  }
  return key;
}

function draw(canvas, empty, metric, data) {
  canvas.hidden = !data;
  get(empty).hidden = !!data;
  get(metric).textContent = data ? data.width + ' × ' + data.height : '';
  if (!data) return;
  canvas.width = data.width;
  canvas.height = data.height;
  canvas.getContext('2d').putImageData(data, 0, 0);
}

function renderStatus() {
  const target = get('status');
  target.textContent = session().message;
  target.dataset.tone = session().tone;
}

function renderControls() {
  const state = session();
  const key = keyFor(state);
  const entries = key ? Arnold.matrix(key.a, key.b, mode) : ['–', '–', '–', '–'];
  ['m11', 'm12', 'm21', 'm22'].forEach((id, index) => { get(id).textContent = entries[index]; });
  get('run-button').disabled = !state.source;
  saveButton.disabled = !state.result;
  transferButton.disabled = !state.result;
  renderStatus();
}

function renderOutput() {
  draw(outputCanvas, 'output-empty', 'output-size', session().result);
  renderControls();
}

function render() {
  const text = copy[mode];
  const state = session();
  get('encrypt-tab').setAttribute('aria-pressed', mode === 'encrypt');
  get('decrypt-tab').setAttribute('aria-pressed', mode === 'decrypt');
  get('page-title').textContent = text.title;
  get('page-description').textContent = text.description;
  get('input-heading').textContent = text.input;
  get('output-heading').textContent = text.output;
  get('input-empty').textContent = text.inputEmpty;
  get('output-empty').textContent = text.outputEmpty;
  get('import-button').textContent = text.fileButton;
  get('sample-button').hidden = mode === 'decrypt';
  transferButton.hidden = mode === 'decrypt';
  get('parameter-note').textContent = text.parameterNote;
  get('condition-note').textContent = text.condition;
  get('matrix-label').textContent = text.matrix;
  get('run-button').textContent = text.run;
  input.accept = mode === 'encrypt' ? 'image/png,image/jpeg,.png,.jpg,.jpeg' : 'image/png,.png';
  get('file-info').textContent = state.source ? state.name : (
    mode === 'encrypt' ? '方形 PNG 或 JPEG · 2–1024 像素 · 无透明像素' : '方形 PNG · 2–1024 像素 · 无透明像素'
  );
  Object.entries(fields).forEach(([name, field]) => { field.value = state.values[name]; });
  draw(inputCanvas, 'input-empty', 'input-size', state.source);
  renderOutput();
}

function show(state, message, tone = '') {
  state.message = message;
  state.tone = tone;
  if (state === session()) renderStatus();
}

function decode(url) {
  return new Promise((resolve, reject) => {
    const image = new Image();
    image.addEventListener('load', () => {
      try {
        const width = image.naturalWidth;
        const height = image.naturalHeight;
        if (width !== height || width < 2 || width > 1024) {
          throw new Error('请选择宽高相同、边长为 2–1024 像素的图片。此图是 ' + width + ' × ' + height + '。');
        }
        const canvas = document.createElement('canvas');
        canvas.width = width;
        canvas.height = height;
        const context = canvas.getContext('2d', { willReadFrequently: true });
        context.drawImage(image, 0, 0);
        const pixels = context.getImageData(0, 0, width, height);
        for (let i = 3; i < pixels.data.length; i += 4) {
          if (pixels.data[i] !== 255) throw new Error('请选择不带透明像素的方形图片。');
        }
        resolve(pixels);
      } catch (error) {
        reject(error);
      }
    }, { once: true });
    image.addEventListener('error', () => reject(new Error('无法读取图像，请换一张 PNG 或 JPEG。')), { once: true });
    image.src = url;
  });
}

async function loadImage(targetMode, url, name) {
  const state = sessions[targetMode];
  const loadId = ++state.loadId;
  show(state, '正在读取图片…');
  try {
    const pixels = await decode(url);
    if (state.loadId !== loadId) return;
    state.source = pixels;
    state.name = name;
    state.result = null;
    state.reference = null;
    show(state, '已载入 ' + pixels.width + ' × ' + pixels.height + ' 图片。');
    if (mode === targetMode) render();
  } catch (error) {
    if (state.loadId === loadId) show(state, error.message, 'error');
  }
}

function identical(left, right) {
  return left.length === right.length && left.every((value, index) => value === right[index]);
}

function runTransform(event) {
  event.preventDefault();
  const state = session();
  const key = keyFor(state);
  if (!state.source || !key) return;
  const { width, data } = state.source;
  const pixels = Arnold.transform(data, width, key, mode);
  state.result = new ImageData(pixels, width, width);

  if (mode === 'encrypt') {
    const same = identical(data, pixels);
    show(state, same ? '当前图像在这组参数和轮数下与原图相同。可调整参数或轮数。'
      : '置乱完成，轮数 ' + key.rounds + '。保存 PNG 时请记下 a、b 和 k。', same ? '' : 'success');
  } else if (state.reference) {
    const same = identical(state.reference.data, pixels);
    show(state, same ? '恢复成功，与原图逐像素一致（' + (width * width).toLocaleString('zh-CN') + ' 个像素）。'
      : '结果与原图不同，请检查 a、b 和轮数 k。', same ? 'success' : 'error');
  } else {
    show(state, '已应用逆变换。可将结果与原图比较来验证参数。');
  }
  renderOutput();
}

function switchMode(target) {
  mode = target;
  render();
}

function transfer() {
  const from = sessions.encrypt;
  if (!from.result) return;
  const to = sessions.decrypt;
  to.loadId += 1;
  to.source = from.result;
  to.name = '来自本次加密的置乱图';
  to.reference = from.source;
  to.result = null;
  to.values = { ...from.values };
  to.message = '已带入置乱图和原参数。运行解密后将与原图逐像素比较。';
  to.tone = '';
  switchMode('decrypt');
}

function savePNG() {
  const state = session();
  const key = keyFor(state);
  if (!state.result || !key) return;
  const suffix = mode === 'encrypt' ? 'scrambled' : 'restored';
  const name = 'arnold-' + suffix + '-a' + key.a + '-b' + key.b + '-k' + key.rounds + '.png';
  const url = outputCanvas.toDataURL('image/png');
  const bridge = window.webkit?.messageHandlers?.savePNG;
  if (bridge) {
    bridge.postMessage({ name, base64: url.split(',', 2)[1] });
    return;
  }
  const download = document.createElement('a');
  download.href = url;
  download.download = name;
  document.body.append(download);
  download.click();
  download.remove();
}

window.arnoldSaveStatus = (message, ok) => show(session(), message, ok ? 'success' : 'error');

get('encrypt-tab').addEventListener('click', () => switchMode('encrypt'));
get('decrypt-tab').addEventListener('click', () => switchMode('decrypt'));
get('sample-button').addEventListener('click', () => loadImage('encrypt', SAMPLE_URL, '示例图 · arnold_sample.png'));
get('import-button').addEventListener('click', () => input.click());
transferButton.addEventListener('click', transfer);
saveButton.addEventListener('click', savePNG);
form.addEventListener('submit', runTransform);

input.addEventListener('change', async () => {
  const file = input.files[0];
  input.value = '';
  if (!file) return;
  const targetMode = mode;
  if (file.size > 24 * 1024 * 1024 || !/\.(png|jpe?g)$/i.test(file.name) ||
      (targetMode === 'decrypt' && !/\.png$/i.test(file.name))) {
    show(session(), targetMode === 'decrypt' ? '请选择不超过 24 MB 的置乱 PNG。'
      : '请选择不超过 24 MB 的 PNG 或 JPEG。', 'error');
    return;
  }
  const url = URL.createObjectURL(file);
  try {
    await loadImage(targetMode, url, file.name);
  } finally {
    URL.revokeObjectURL(url);
  }
});

Object.entries(fields).forEach(([name, field]) => {
  field.addEventListener('input', () => {
    const state = session();
    state.values[name] = field.value;
    state.result = null;
    show(state, '参数已更改，重新运行' + (mode === 'encrypt' ? '加密' : '解密') + '后查看结果。');
    renderOutput();
  });
});

render();
loadImage('encrypt', SAMPLE_URL, '示例图 · arnold_sample.png');

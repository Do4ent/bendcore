// Strings of the drawing editor (docs/ui/DRAWING_UI.md). Merged in src/ui/app/i18n.ts.

export const drawingRu = {
  'drawing.file.open': 'Открыть чертёж…',
  'drawing.file.openHint': 'Файл чертежа .bcdrw.json (или чертёж .drawing.json прежнего формата вместе с файлом его модели)',
  'drawing.file.replaceModel': 'Заменить модель…',
  'drawing.file.replaceModelHint': 'Загрузить чертёж на более новый файл его модели: отчёт покажет изменённые и висячие размеры',
  'drawing.file.needsModel': 'Чертёж прежнего формата (.drawing.json) открывается вместе с файлом его модели: выберите оба файла.',
  'drawing.file.migrated': 'Открыт чертёж прежнего формата; при сохранении он будет записан одним файлом {name}',
  'drawing.file.saved': 'Сохранено: {name} (чертёж с моделью)',
  'drawing.file.modelReplaced': 'Модель заменена файлом {name}',
} as const;

export const drawingEn: Record<keyof typeof drawingRu, string> = {
  'drawing.file.open': 'Open drawing…',
  'drawing.file.openHint': 'Drawing file .bcdrw.json (or a .drawing.json of the earlier format together with its model file)',
  'drawing.file.replaceModel': 'Replace model…',
  'drawing.file.replaceModelHint': 'Load the drawing on a newer file of its model: the report lists changed and dangling dimensions',
  'drawing.file.needsModel': 'A drawing of the earlier format (.drawing.json) opens together with its model file: choose both files.',
  'drawing.file.migrated': 'A drawing of the earlier format was opened; saving writes it as one file {name}',
  'drawing.file.saved': 'Saved: {name} (drawing with its model)',
  'drawing.file.modelReplaced': 'The model was replaced by {name}',
};

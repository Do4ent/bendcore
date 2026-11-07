// Фикстура самотеста check_no_stubs: тест Jest с двойником ядра — законный код.
import { faceArea } from '../good_viewer';

const mockApi = {
  volume: (handle: number) => 6000 * handle,
  dispose: (handle: number) => {},
};

test('face area is half of the reported volume', () => {
  // Двойник возвращает fake data — в тестах это законно.
  expect(faceArea(mockApi, 2)).toBeCloseTo(6000, 9);
});

import type { LearnInStateData } from './LearnInStateData';

interface LearnInStateEventEnvelope {
  'event': 'learn_in_state';
  'data': LearnInStateData;
}
export { LearnInStateEventEnvelope };
